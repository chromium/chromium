-- unit_tests --gtest_filter=FaviconDatabaseTest.WildSchema
--
-- Based on version 3 schema found in the wild by error diagnostics.
-- The schema was failing to migrate because the current-version
-- tables were being created before the migration code was called,
-- resulting in the migration code attempting to add columns which
-- already existed (see http://crbug.com/273203 ).
--
-- Should be razed by the deprecation code.
BEGIN TRANSACTION;

-- [meta] is expected.
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','3');
INSERT INTO "meta" VALUES('last_compatible_version','3');

-- [thumbnails] is optional for v3, but was not seen in any diagnostic
-- results.

-- [favicons] was present in v3, but [icon_type] is a v4 column.
CREATE TABLE favicons(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,icon_type INTEGER DEFAULT 1);
CREATE INDEX favicons_url ON favicons(url);

-- [icon_mapping] is a v4 table.  Some cases didn't have the indices,
-- possibly because the migration code didn't create indices (they
-- were created optimistically on next run).
CREATE TABLE icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);
CREATE INDEX icon_mapping_icon_id_idx ON icon_mapping(icon_id);
CREATE INDEX icon_mapping_page_url_idx ON icon_mapping(page_url);

-- [favicon_bitmaps] is a v6 table.  Some diagnostic results did not
-- contain this table.
CREATE TABLE favicon_bitmaps(id INTEGER PRIMARY KEY,icon_id INTEGER NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,width INTEGER DEFAULT 0,height INTEGER DEFAULT 0);
CREATE INDEX favicon_bitmaps_icon_id ON favicon_bitmaps(icon_id);

COMMIT;
