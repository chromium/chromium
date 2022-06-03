-- unit_tests --gtest_filter=FaviconDatabaseTest.WildSchema
--
-- Based on version 5 schema found in the wild by error diagnostics.
-- The schema failed migrations because of an unexpected
-- [temp_favicons] table (see http://crbug.com/272519 ).
--
-- Init() has been modified to drop these tables before migration.
BEGIN TRANSACTION;

-- [meta] is expected.
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','5');
INSERT INTO "meta" VALUES('last_compatible_version','5');

-- This version of [favicons] is consistent with v5.
CREATE TABLE "favicons"(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,icon_type INTEGER DEFAULT 1,sizes LONGVARCHAR);
CREATE INDEX favicons_url ON favicons(url);

-- [icon_mapping] consistent with v5.
CREATE TABLE icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);
CREATE INDEX icon_mapping_icon_id_idx ON icon_mapping(icon_id);
CREATE INDEX icon_mapping_page_url_idx ON icon_mapping(page_url);

-- [favicon_bitmaps] is a v6 table created by Init() before version
-- check.  This probably is only in memory (not committed).
CREATE TABLE favicon_bitmaps(id INTEGER PRIMARY KEY,icon_id INTEGER NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,width INTEGER DEFAULT 0,height INTEGER DEFAULT 0);
CREATE INDEX favicon_bitmaps_icon_id ON favicon_bitmaps(icon_id);

-- Presence of these tables is consistent with an aborted attempt to
-- clear history.  Prior to r217993, that code was not contained in a
-- transaction (or possibly there was a non-atomic update).
CREATE TABLE temp_favicons(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB,icon_type INTEGER DEFAULT 1,sizes LONGVARCHAR);
CREATE TABLE temp_icon_mapping(id INTEGER PRIMARY KEY,page_url LONGVARCHAR NOT NULL,icon_id INTEGER);

COMMIT;
