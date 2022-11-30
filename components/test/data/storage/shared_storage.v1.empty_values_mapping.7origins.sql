-- components_unittests --gtest_filter=SharedStorageDatabaseTest.SevenOrigins
--
-- .dump of a version 1 Shared Storage database.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','1');
INSERT INTO "meta" VALUES('last_compatible_version','1');
CREATE TABLE values_mapping(context_origin TEXT NOT NULL,key TEXT NOT NULL,value TEXT,PRIMARY KEY(context_origin,key)) WITHOUT ROWID;
CREATE TABLE per_origin_mapping(context_origin TEXT NOT NULL PRIMARY KEY,last_used_time INTEGER NOT NULL,length INTEGER NOT NULL) WITHOUT ROWID;
INSERT INTO "per_origin_mapping" VALUES ('http://google.com',13266954476192362,20);
INSERT INTO "per_origin_mapping" VALUES ('http://chromium.org',13268941676192362,40);
INSERT INTO "per_origin_mapping" VALUES ('http://gv.com',13268941793856733,15);
INSERT INTO "per_origin_mapping" VALUES ('http://abc.xyz',13269481776356965,250);
INSERT INTO "per_origin_mapping" VALUES ('http://withgoogle.com',13269545986263676,1001);
INSERT INTO "per_origin_mapping" VALUES ('http://waymo.com',13269546064355176,1599);
INSERT INTO "per_origin_mapping" VALUES ('http://google.org',13269546476192362,10);
CREATE TABLE budget_mapping(id INTEGER NOT NULL PRIMARY KEY,context_origin TEXT NOT NULL,time_stamp INTEGER NOT NULL,bits_debit REAL NOT NULL);
CREATE INDEX IF NOT EXISTS per_origin_mapping_last_used_time_idx ON per_origin_mapping(last_used_time);
CREATE INDEX IF NOT EXISTS budget_mapping_origin_time_stamp_idx ON budget_mapping(context_origin,time_stamp);
COMMIT;
