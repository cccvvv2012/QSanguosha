#include "miniscenarios.h"

#include <QFile>

MiniSceneRule::MiniSceneRule(Scenario *scenario)
    :ScenarioRule(scenario)
{
    events << GameStart << PhaseChange;
}

void MiniSceneRule::assign(QStringList &generals, QStringList &roles) const{
    for(int i=0;i<players.length();i++)
    {
        QMap<QString,QString> sp =players.at(i);
        QString name = sp["general"];
        if(name == "select")name = "sujiang";
        generals << name;
        roles << sp["role"];
    }
}

QStringList MiniSceneRule::existedGenerals() const
{
    QStringList names;
    for(int i=0;i<players.length();i++)
    {
        QMap<QString,QString> sp =players.at(i);
        QString name = sp["general"];
        if(name == "select")name = "sujiang";
        names << name;
        name = sp["general2"];
        if(name == NULL )continue;
        if(name == "select")name = "sujiang";
        names << name;
    }
    return names;
}

bool MiniSceneRule::trigger(TriggerEvent event, ServerPlayer *player, QVariant &data) const
{
    Room* room = player->getRoom();

    if(event == PhaseChange)
    {
        if(player->getPhase()==Player::Start && this->players.first()["beforeNext"] != NULL
                )
        {
            if(player->tag["playerHasPlayed"].toBool())
                room->gameOver(this->players.first()["beforeNext"]);
            else player->tag["playerHasPlayed"] = true;
        }

        if(player->getPhase() != Player::NotActive)return false;
        if(player->getState() == "robot" || this->players.first()["singleTurn"] == NULL)
            return false;
        room->gameOver(this->players.first()["singleTurn"]);
    }
    if(player->getRoom()->getTag("WaitForPlayer").toBool())
        return true;

    QList<ServerPlayer*> players = room->getAllPlayers();
    while(players.first()->getState() == "robot")
        players.append(players.takeFirst());

    QStringList cards= setup.split(",");
    foreach(QString id,cards)
    {
        room->moveCardTo(Sanguosha->getCard(id.toInt()),NULL,Player::Special,true);
        room->moveCardTo(Sanguosha->getCard(id.toInt()),NULL,Player::DrawPile,true);
        room->broadcastInvoke("addHistory","pushPile");
    }

    int i=0;
    foreach(ServerPlayer * sp,players)
    {
        room->setPlayerProperty(sp,"role",this->players.at(i)["role"]);

        if(sp->getState()!= "robot")
        {
            QString general = this->players.at(i)["general"];
            {
                QString original = sp->getGeneralName();
                if(general == "select")
                {
                    QStringList available,all,existed;
                    existed = existedGenerals();
                    all = Sanguosha->getRandomGenerals(Sanguosha->getGeneralCount());
                    qShuffle(all);
                    for(int i=0;i<5;i++)
                    {
                        sp->setGeneral(NULL);
                        QString choice = sp->findReasonable(all);
                        if(existed.contains(choice))
                        {
                            all.removeOne(choice);
                            i--;
                            continue;
                        }
                        available << choice;
                        all.removeOne(choice);
                    }
                    general = room->askForGeneral(sp,available);
                }
                QString trans = QString("%1:%2").arg(original).arg(general);
                sp->invoke("transfigure",trans);
                room->setPlayerProperty(sp,"general",general);
            }
            general = this->players.at(i)["general2"];
            {
                if(general == "select")
                {
                    QStringList available,all,existed;
                    existed = existedGenerals();
                    all = Sanguosha->getRandomGenerals(Sanguosha->getGeneralCount());
                    qShuffle(all);
                    for(int i=0;i<5;i++)
                    {
                        room->setPlayerProperty(sp,"general2",NULL);
                        QString choice = sp->findReasonable(all);
                        if(existed.contains(choice))
                        {
                            all.removeOne(choice);
                            i--;
                            continue;
                        }
                        available << choice;
                        all.removeOne(choice);
                    }
                    general = room->askForGeneral(sp,available);
                }
                if(general == sp->getGeneralName())general = this->players.at(i)["general3"];
                QString trans = QString("%1:%2").arg("sujiang").arg(general);
                sp->invoke("transfigure",trans);
                room->setPlayerProperty(sp,"general2",general);
            }
        }
        else
        {
            room->setPlayerProperty(sp,"general",this->players.at(i)["general"]);
            if(this->players.at(i)["general2"]!=NULL)room->setPlayerProperty(sp,"general2",this->players.at(i)["general2"]);
        }

        room->setPlayerProperty(sp,"kingdom",sp->getGeneral()->getKingdom());

        QString str = this->players.at(i)["maxhp"];
        if(str==NULL)str=QString::number(sp->getGeneralMaxHP());
        room->setPlayerProperty(sp,"maxhp",str.toInt());

        str = this->players.at(i)["hpadj"];
        if(str!=NULL)
            room->setPlayerProperty(sp,"maxhp",sp->getMaxHP()+str.toInt());
        str=QString::number(sp->getMaxHP());

        QString str2 = this->players.at(i)["hp"];
        if(str2 != NULL)str = str2;
        room->setPlayerProperty(sp,"hp",str.toInt());

        str = this->players.at(i)["equip"];
        QStringList equips = str.split(",");
        foreach(QString equip,equips)
        {
            bool ok;
            equip.toInt(&ok);
            if(!ok)room->installEquip(sp,equip);
            else room->moveCardTo(Sanguosha->getCard(equip.toInt()),sp,Player::Equip);
        }

        str = this->players.at(i)["judge"];
        if(str != NULL)
        {
            QStringList judges = str.split(",");
            foreach(QString judge,judges)
            {
                room->moveCardTo(Sanguosha->getCard(judge.toInt()),sp,Player::Judging);
            }
        }

        str = this->players.at(i)["hand"];
        if(str !=NULL)
        {
            QStringList hands = str.split(",");
            foreach(QString hand,hands)
            {
                room->obtainCard(sp,hand.toInt());
            }

        }

        QVariant v;
        foreach(const TriggerSkill *skill, sp->getTriggerSkills()){
            if(!skill->inherits("SPConvertSkill"))
                room->getThread()->addTriggerSkill(skill);
            else continue;

            if(skill->getTriggerEvents().contains(GameStart))
                skill->trigger(GameStart, sp, v);
        }

        if(this->players.at(i)["chained"] != NULL){
            sp->setChained(true);
            room->broadcastProperty(sp, "chained");
            room->setEmotion(sp, "chain");
        }
        if(this->players.at(i)["turned"] == "true"){
            if(sp->faceUp())
                sp->turnOver();
        }
        if(this->players.at(i)["starter"] != NULL)
            room->setCurrent(sp);

        i++;
    }
    i =0;
    foreach(ServerPlayer *sp,players)
    {
        QString str = this->players.at(i)["draw"];
        if(str == NULL)str = "4";
        room->drawCards(sp,str.toInt());

        if(this->players.at(i)["marks"] != NULL)
        {
            QStringList marks = this->players.at(i)["marks"].split(",");
            foreach(QString qs,marks)
            {
                QStringList keys = qs.split("*");
                str = keys.at(1);
                sp->gainMark(keys.at(0),str.toInt());
            }
        }

        i++;
    }

    room->setTag("WaitForPlayer",QVariant(true));
    room->updateStateItem();
    return true;
}

void MiniSceneRule::addNPC(QString feature)
{
    QMap<QString, QString> player;
    QStringList features;
    if(feature.contains("|"))features= feature.split("|");
    else features = feature.split(" ");
    foreach(QString str, features)
    {
        QStringList keys = str.split(":");
        if(keys.size()<2)continue;
        if(keys.first().size()<1)continue;
        player.insert(keys.at(0),keys.at(1));
    }

    players << player;
}

void MiniSceneRule::setPile(QString cardList)
{
    setup = cardList;
}

void MiniSceneRule::loadSetting(QString path)
{
    QFile file(path);
    if(file.open(QIODevice::ReadOnly)){
        players.clear();
        setup.clear();

        QTextStream stream(&file);
        while(!stream.atEnd()){
            QString aline = stream.readLine();
            if(aline.startsWith("setPile"))
                setPile(aline.split(":").at(1));
            else
                addNPC(aline);
        }
        file.close();
    }
}

MiniScene::MiniScene(const QString &name)
    :Scenario(name){
    rule = new MiniSceneRule(this);
}

void MiniScene::setupCustom(QString name) const
{
    if(name == NULL)name = "custom_scenario";
    name.prepend("etc/customScenes/");
    name.append(".txt");

    MiniSceneRule* arule = qobject_cast<MiniSceneRule*>(this->getRule());
    arule->loadSetting(name);

}

void MiniScene::onTagSet(Room *room, const QString &key) const
{

}
#define ADD_CUSTOM_SCENARIO(name) extern "C" { Q_DECL_EXPORT Scenario *NewMiniScene_##name() { return new LoadedScenario(#name); } }

ADD_CUSTOM_SCENARIO(01)
ADD_CUSTOM_SCENARIO(02)
ADD_CUSTOM_SCENARIO(03)
ADD_CUSTOM_SCENARIO(04)
ADD_CUSTOM_SCENARIO(05)
ADD_CUSTOM_SCENARIO(06)
ADD_CUSTOM_SCENARIO(07)
ADD_CUSTOM_SCENARIO(08)
ADD_CUSTOM_SCENARIO(09)
ADD_CUSTOM_SCENARIO(10)
ADD_CUSTOM_SCENARIO(11)
ADD_CUSTOM_SCENARIO(12)
ADD_CUSTOM_SCENARIO(13)
ADD_CUSTOM_SCENARIO(14)
ADD_CUSTOM_SCENARIO(15)
ADD_CUSTOM_SCENARIO(16)
ADD_CUSTOM_SCENARIO(17)
ADD_CUSTOM_SCENARIO(18)
ADD_CUSTOM_SCENARIO(19)
ADD_CUSTOM_SCENARIO(20)

ADD_SCENARIO(Custom)