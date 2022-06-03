-- unit_tests --gtest_filter=FaviconDatabaseTest.WildSchema
--
-- Based on version 2 schema found in the wild by error diagnostics.
-- The schema was failing to migrate because the current-version
-- tables were being created before the migration code was called,
-- resulting in the migration code attempting to add columns which
-- already existed (see http://crbug.com/273203 ).
--
-- Should be razed by the deprecation code.
BEGIN TRANSACTION;

-- [meta] and [thumbnails] are expected tables.
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','2');
INSERT INTO "meta" VALUES('last_compatible_version','2');
CREATE TABLE thumbnails (url LONGVARCHAR PRIMARY KEY,url_rank INTEGER ,title LONGVARCHAR,thumbnail BLOB,redirects LONGVARCHAR,boring_score DOUBLE DEFAULT 1.0, good_clipping INTEGER DEFAULT 0, at_top INTEGER DEFAULT 0, last_updated INTEGER DEFAULT 0, load_completed INTEGER DEFAULT 0);

-- Tables optimistically created by Init().
CREATE TABLE favicon_bitmaps(id INTEGER PRIMARY KEY,icon_id INTEGER NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,width INTEGER DEFAULT 0,height INTEGER DEFAULT 0);
CREATE TABLE favicons(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,icon_type INTEGER DEFAULT 1);
CREATE TABLE icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);
CREATE INDEX favicon_bitmaps_icon_id ON favicon_bitmaps(icon_id);
CREATE INDEX favicons_url ON favicons(url);
CREATE INDEX icon_mapping_icon_id_idx ON icon_mapping(icon_id);
CREATE INDEX icon_mapping_page_url_idx ON icon_mapping(page_url);

COMMIT;
