-- unit_tests --gtest_filter=FaviconDatabaseTest.Version3
--
-- .dump of a version 3 Favicons database.  Version 3 contained a
-- table [thumbnails] which was migrated to [Top Sites] database, and
-- the [favicons] rows were referenced by the [urls.favicon_id] table
-- in this history database.  See Favicons.v3.history.sql.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','3');
INSERT INTO "meta" VALUES('last_compatible_version','3');
CREATE TABLE favicons(id INTEGER PRIMARY KEY,url LONGVARCHAR NOT NULL,last_updated INTEGER DEFAULT 0,image_data BLOB);
INSERT INTO "favicons" VALUES(1,'http://www.google.com/favicon.ico',1287424416,X'313233343631303233353631323033393437353136333435313635393133343837313034373831323336343931363534313932333435313932333435313233343931333400');
INSERT INTO "favicons" VALUES(2,'http://www.yahoo.com/favicon.ico',1287424428,X'676F6977756567727172636F6D697A71797A6B6A616C697462616878666A7974727176707165726F6963786D6E6C6B686C7A756E616378616E65766961777274786379776867656600');
CREATE TABLE thumbnails(url_id INTEGER PRIMARY KEY,boring_score DOUBLE DEFAULT 1.0,good_clipping INTEGER DEFAULT 0,at_top INTEGER DEFAULT 0,last_updated INTEGER DEFAULT 0,data BLOB);
CREATE INDEX favicons_url ON favicons(url);
COMMIT;
