-- components_unittests --gtest_filter=TopSitesDatabaseTest.Version5
--
-- .dump of a version 5 "Top Sites" database.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','5');
INSERT INTO "meta" VALUES('last_compatible_version','5');
CREATE TABLE top_sites (url TEXT NOT NULL PRIMARY KEY,url_rank INTEGER NOT NULL,title TEXT NOT NULL);
INSERT INTO "top_sites" VALUES('http://www.google.com/chrome/intl/en/welcome.html',1,'Welcome to Chromium');
INSERT INTO "top_sites" VALUES('https://chrome.google.com/webstore?hl=en',2,'Chrome Web Store');
INSERT INTO "top_sites" VALUES('http://www.google.com/',0,'Google');
COMMIT;
