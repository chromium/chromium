-- components_unittests --gtest_filter=SharedStorageDatabaseTest.SingleOrigin
--
-- .dump of a version 2 Shared Storage database.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','2');
INSERT INTO "meta" VALUES('last_compatible_version','1');
CREATE TABLE IF NOT EXISTS "values_mapping"(context_origin TEXT NOT NULL,key TEXT NOT NULL,value TEXT,last_used_time INTEGER NOT NULL,PRIMARY KEY(context_origin,key)) WITHOUT ROWID;
INSERT INTO "values_mapping" VALUES ('http://google.com','key1','value1',13312097333991364);
INSERT INTO "values_mapping" VALUES ('http://google.com','key2','value2',13313037427966159);
INSERT INTO "values_mapping" VALUES ('http://google.com','key3','value3',13313037435619704);
INSERT INTO "values_mapping" VALUES ('http://google.com','key4','value4',13313037416916308);
INSERT INTO "values_mapping" VALUES ('http://google.com','key5','value5',13313037416916308);
INSERT INTO "values_mapping" VALUES ('http://google.com','key6','value6',13312097333991364);
INSERT INTO "values_mapping" VALUES ('http://google.com','key7','value7',13312353831182651);
INSERT INTO "values_mapping" VALUES ('http://google.com','key8','value8',13313037487092131);
INSERT INTO "values_mapping" VALUES ('http://google.com','key9','value9',13269481776356965);
INSERT INTO "values_mapping" VALUES ('http://google.com','key10','value10',13269481776356965);
CREATE TABLE per_origin_mapping(context_origin TEXT NOT NULL PRIMARY KEY,creation_time INTEGER NOT NULL,length INTEGER NOT NULL) WITHOUT ROWID;
INSERT INTO "per_origin_mapping" VALUES ('http://google.com',13266954476192362,10);
CREATE TABLE budget_mapping(id INTEGER NOT NULL PRIMARY KEY,context_origin TEXT NOT NULL,time_stamp INTEGER NOT NULL,bits_debit REAL NOT NULL);
CREATE INDEX budget_mapping_origin_time_stamp_idx ON budget_mapping(context_origin,time_stamp);
CREATE INDEX values_mapping_last_used_time_idx ON values_mapping(last_used_time);
CREATE INDEX per_origin_mapping_creation_time_idx ON per_origin_mapping(creation_time);
COMMIT;
