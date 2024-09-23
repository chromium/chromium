-- components_unittests --gtest_filter=SharedStorageDatabaseTest.DestroyTooNew
--
-- .dump of a version 1 Shared Storage database.
-- intentionally set up to fail initialization with error SharedStorageDatabase::InitStatus::kTooNew
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','1');
INSERT INTO "meta" VALUES('last_compatible_version','7');
CREATE TABLE values_mapping(context_origin TEXT NOT NULL,key TEXT NOT NULL,value TEXT,PRIMARY KEY(context_origin,key)) WITHOUT ROWID;
INSERT INTO "values_mapping" VALUES ('http://google.com','key1','value1');
INSERT INTO "values_mapping" VALUES ('http://google.com','key2','value2');
CREATE TABLE per_origin_mapping(context_origin TEXT NOT NULL PRIMARY KEY,last_used_time INTEGER NOT NULL,length INTEGER NOT NULL) WITHOUT ROWID;
CREATE TABLE budget_mapping(id INTEGER NOT NULL PRIMARY KEY,context_origin TEXT NOT NULL,time_stamp INTEGER NOT NULL,bits_debit REAL NOT NULL);
INSERT INTO "per_origin_mapping" VALUES ('http://google.com',13266954476192362,2);
CREATE INDEX IF NOT EXISTS per_origin_mapping_last_used_time_idx ON per_origin_mapping(last_used_time);
CREATE INDEX IF NOT EXISTS budget_mapping_origin_time_stamp_idx ON budget_mapping(context_origin,time_stamp);
COMMIT;
