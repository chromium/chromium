-- unit_tests --gtest_filter=FaviconDatabaseTest.WildSchema
--
-- Based on version 3 and 4 schema found in the wild by error
-- diagnostics.  Combined because they were the same except for
-- version.  The schema was failing to migrate because the migration
-- code encountered a table which appeared to already be migrated.
--
-- Should be razed by the deprecation code.
BEGIN TRANSACTION;

-- [meta] is expected.
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','4');
INSERT INTO "meta" VALUES('last_compatible_version','4');

-- This version of [favicons] implies v6 code.  Other versions are
-- missing [size], implying v7.  Either way, since v4 had the table,
-- migration must have run, but the atomic update of version somehow
-- failed.
CREATE TABLE "favicons"(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,icon_type INTEGER DEFAULT 1,sizes LONGVARCHAR);
CREATE INDEX favicons_url ON favicons(url);

-- [icon_mapping] is a v4 table.
CREATE TABLE icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);
CREATE INDEX icon_mapping_icon_id_idx ON icon_mapping(icon_id);
CREATE INDEX icon_mapping_page_url_idx ON icon_mapping(page_url);

-- [favicon_bitmaps] is a v6 table.
CREATE TABLE favicon_bitmaps(id INTEGER PRIMARY KEY,icon_id INTEGER NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,width INTEGER DEFAULT 0,height INTEGER DEFAULT 0);
CREATE INDEX favicon_bitmaps_icon_id ON favicon_bitmaps(icon_id);

COMMIT;
