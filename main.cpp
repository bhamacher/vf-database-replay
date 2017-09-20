#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>
#include <QMimeDatabase>

#include <ve_eventhandler.h>
#include <ve_eventsystem.h>
#include <vs_veinhash.h>
#include <vn_introspectionsystem.h>
#include <vn_networksystem.h>
#include <vn_tcpsystem.h>

#include "databasereplaysystem.h"

bool checkDatabaseParam(const QString &t_dbParam)
{
  bool retVal = false;
  if(t_dbParam.isEmpty())
  {
    qWarning() << "invalid parameter f" << t_dbParam;
  }
  else
  {
    QFileInfo dbFInfo;
    dbFInfo.setFile(t_dbParam);

    if(dbFInfo.exists() == false)
    {
      qWarning() << "Database file does not exist:" << t_dbParam;
    }
    else
    {
      QMimeDatabase mimeDB;
      const QString mimeName = mimeDB.mimeTypeForFile(dbFInfo, QMimeDatabase::MatchContent).name();
      if(mimeName != "application/x-sqlite3")
      {
        qWarning() << "Database filetype not supported:" << mimeName;
      }
      else
      {
        retVal = true;
        qDebug() << "Database file:"  << t_dbParam << QString("%1 MB").arg(dbFInfo.size()/1024.0/1024.0);
      }
    }
  }

  return retVal;
}

bool checkTickrateParam(const QString &t_tickrate)
{
  bool retVal = false;
  bool tickrateOk = false;
  const int tickrate = t_tickrate.toInt(&tickrateOk);
  if(tickrateOk == false || tickrate < 10 || tickrate > 1000)
  {
    qWarning() << "Invalid parameter t" << t_tickrate;
  }
  else
  {
    retVal = true;
    qDebug() << "tickrate:" << tickrate;
  }

  return retVal;
}

int main(int argc, char *argv[])
{
  QStringList loggingFilters = QStringList() << QString("%1.debug=false").arg(VEIN_EVENT().categoryName()) <<
                                                QString("%1.debug=false").arg(VEIN_NET_VERBOSE().categoryName()) <<
//                                                QString("%1.debug=false").arg(VEIN_NET_INTRO_VERBOSE().categoryName()) << //< Introspection logging is still enabled
                                                QString("%1.debug=false").arg(VEIN_NET_TCP_VERBOSE().categoryName()) <<
                                                QString("%1.debug=false").arg(VEIN_STORAGE_HASH_VERBOSE().categoryName());


  QLoggingCategory::setFilterRules(loggingFilters.join("\n"));

  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("vf-database-replay");
  QCoreApplication::setApplicationVersion("0.1");

  QCommandLineParser parser;
  parser.setApplicationDescription("Reads specially formated SQLite databases and replays the data from the value");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption databaseOption("f",
                                    QCoreApplication::translate("main", "SQLite 3 database file to read from"),
                                    QCoreApplication::translate("main", "database file"));


  QCommandLineOption tickrateOption("t",
                                    QCoreApplication::translate("main", "Frequency of updates as integer ms value, 10 <= tickrate <= 1000"),
                                    QCoreApplication::translate("main", "tickrate"));

  QCommandLineOption loopOption("l", QCoreApplication::translate("main", "Loop over data until interrupted"));
  parser.addOption(loopOption);

  parser.addOption(databaseOption);
  parser.addOption(tickrateOption);
  parser.process(app);

  if(checkDatabaseParam(parser.value(databaseOption)) == false)
  {
    //return with exit 1
    parser.showHelp(1);
  }

  if(checkTickrateParam(parser.value(tickrateOption)) == false)
  {
    //return with exit 1
    parser.showHelp(1);
  }

  VeinEvent::EventHandler evHandler;
  DatabaseReplaySystem *replaySystem = new DatabaseReplaySystem(&app);
  VeinStorage::VeinHash *storSystem = new VeinStorage::VeinHash(&app);
  VeinNet::IntrospectionSystem *introspectionSystem = new VeinNet::IntrospectionSystem(storSystem, &app);
  VeinNet::NetworkSystem *netSystem = new VeinNet::NetworkSystem(&app);
  VeinNet::TcpSystem *tcpSystem = new VeinNet::TcpSystem(&app);

  netSystem->setOperationMode(VeinNet::NetworkSystem::VNOM_PASS_THROUGH);

  QList<VeinEvent::EventSystem*> subSystems;
  subSystems.append(replaySystem);
  subSystems.append(storSystem);
  subSystems.append(introspectionSystem);
  subSystems.append(netSystem);
  subSystems.append(tcpSystem);

  evHandler.setSubsystems(subSystems);

  replaySystem->setDatabaseFile(parser.value(databaseOption));
  replaySystem->setTickrate(parser.value(tickrateOption).toInt());
  replaySystem->startReplay();
  replaySystem->setLoop(parser.isSet(loopOption));

  tcpSystem->startServer(12000);

  return app.exec();
}
