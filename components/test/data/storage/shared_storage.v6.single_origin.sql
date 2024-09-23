-- components_unittests --gtest_filter=SharedStorageDatabaseTest.SingleOrigin
--
-- .dump of a version 6 Shared Storage database.
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('last_compatible_version','4');
INSERT INTO meta VALUES('version','6');
CREATE TABLE IF NOT EXISTS "values_mapping"(context_origin TEXT NOT NULL,key BLOB NOT NULL,value BLOB NOT NULL,last_used_time INTEGER NOT NULL,PRIMARY KEY(context_origin,key)) WITHOUT ROWID;
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003100',X'760061006c00750065003100',13312097333991364);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003200',X'760061006c00750065003200',13313037427966159);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003300',X'760061006c00750065003300',13313037435619704);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003400',X'760061006c00750065003400',13313037416916308);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003500',X'760061006c00750065003500',13313037416916308);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003600',X'760061006c00750065003600',13312097333991364);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003700',X'760061006c00750065003700',13312353831182651);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003800',X'760061006c00750065003800',13313037487092131);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003900',X'760061006c00750065003900',13269481776356965);
INSERT INTO "values_mapping" VALUES ('http://google.com',X'6b00650079003a00',X'760061006c00750065003a00',13269481776356999);
CREATE TABLE IF NOT EXISTS "per_origin_mapping"(context_origin TEXT NOT NULL PRIMARY KEY,creation_time INTEGER NOT NULL,num_bytes INTEGER NOT NULL) WITHOUT ROWID;
INSERT INTO "per_origin_mapping" VALUES ('http://google.com',13266954476192362,200);
CREATE TABLE budget_mapping(id INTEGER NOT NULL PRIMARY KEY,context_site TEXT NOT NULL,time_stamp INTEGER NOT NULL,bits_debit REAL NOT NULL);
CREATE INDEX budget_mapping_site_time_stamp_idx ON budget_mapping(context_site,time_stamp);
CREATE INDEX values_mapping_last_used_time_idx ON values_mapping(last_used_time);
CREATE INDEX per_origin_mapping_creation_time_idx ON per_origin_mapping(creation_time);
COMMIT;
