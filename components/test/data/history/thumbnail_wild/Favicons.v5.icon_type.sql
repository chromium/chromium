-- unit_tests --gtest_filter=FaviconDatabaseTest.WildSchema
--
-- Based on version 5 schema found in the wild by error diagnostics.
-- The schema failed to open because the v3 [favicons] table is
-- missing [icon_type].
--
-- Init() should clean up with RazeAndClose().
BEGIN TRANSACTION;

-- [meta] is expected.
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','5');
INSERT INTO "meta" VALUES('last_compatible_version','5');

-- v3 [favicons] (missing [icon_type]), but v5 database.  
CREATE TABLE "favicons"(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB);
CREATE INDEX favicons_url ON favicons(url);

-- [icon_mapping] consistent with v5.
CREATE TABLE icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);
CREATE INDEX icon_mapping_icon_id_idx ON icon_mapping(icon_id);
CREATE INDEX icon_mapping_page_url_idx ON icon_mapping(page_url);

COMMIT;
