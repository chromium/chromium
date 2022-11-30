-- components_unittests --gtest_filter=SharedStorageDatabaseTest.SingleOrigin
--
-- .dump of a version 1 Shared Storage database.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','1');
INSERT INTO "meta" VALUES('last_compatible_version','1');
CREATE TABLE values_mapping(context_origin TEXT NOT NULL,key TEXT NOT NULL,value TEXT,PRIMARY KEY(context_origin,key)) WITHOUT ROWID;
INSERT INTO "values_mapping" VALUES ('http://google.com','key1','value1');
INSERT INTO "values_mapping" VALUES ('http://google.com','key2','value2');
INSERT INTO "values_mapping" VALUES ('http://google.com','key3','value3');
INSERT INTO "values_mapping" VALUES ('http://google.com','key4','value4');
INSERT INTO "values_mapping" VALUES ('http://google.com','key5','value5');
INSERT INTO "values_mapping" VALUES ('http://google.com','key6','value6');
INSERT INTO "values_mapping" VALUES ('http://google.com','key7','value7');
INSERT INTO "values_mapping" VALUES ('http://google.com','key8','value8');
INSERT INTO "values_mapping" VALUES ('http://google.com','key9','value9');
INSERT INTO "values_mapping" VALUES ('http://google.com','key10','value10');
CREATE TABLE per_origin_mapping(context_origin TEXT NOT NULL PRIMARY KEY,last_used_time INTEGER NOT NULL,length INTEGER NOT NULL) WITHOUT ROWID;
INSERT INTO "per_origin_mapping" VALUES ('http://google.com',13266954476192362,10);
CREATE TABLE budget_mapping(id INTEGER NOT NULL PRIMARY KEY,context_origin TEXT NOT NULL,time_stamp INTEGER NOT NULL,bits_debit REAL NOT NULL);
CREATE INDEX IF NOT EXISTS per_origin_mapping_last_used_time_idx ON per_origin_mapping(last_used_time);
CREATE INDEX IF NOT EXISTS budget_mapping_origin_time_stamp_idx ON budget_mapping(context_origin,time_stamp);
COMMIT;
