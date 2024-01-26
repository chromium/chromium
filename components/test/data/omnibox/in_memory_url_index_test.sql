-- This file contains test data used
-- chrome/browser/history/in_memory_url_index_unittest.cc
--
-- It contains data for two types of history database tables:
-- 1. the history URL database.
-- 2. the visit database.
--
-- 1.
-- The schema of the URL database is defined by HISTORY_URL_ROW_FIELDS found in
-- url_database.h and is equivalent to:
--
-- CREATE TABLE urls(id INTEGER PRIMARY KEY AUTOINCREMENT,
--                   url LONGVARCHAR,
--                   title LONGVARCHAR,
--                   visit_count INTEGER DEFAULT 0 NOT NULL,
--                   typed_count INTEGER DEFAULT 0 NOT NULL,
--                   last_visit_time INTEGER NOT NULL,
--                   hidden INTEGER DEFAULT 0 NOT NULL);
--
-- The quick history autocomplete provider filters out history items that:
--   1) have not been visited in kLowQualityMatchAgeLimitInDays, AND
--   2) for which the URL was not explicitly typed at least
--      kLowQualityMatchTypedLimit + 1 times, AND
--   3) have not been visited at least kLowQualityMatchVisitLimit + 1 times.
-- So we create history items in all of those combinations.
--
-- Note that the last_visit_time column for this test table represents the
-- relative number of days prior to 'today' to which the final column
-- value will be set during test setup. Beware: Do not set this number
-- to be equal to kLowQualityMatchAgeLimitInDays.
--
-- The ordering, URLs and titles must be kept in sync with the unit tests found
-- in in_memory_url_index_unittest.cc.
--
-- 2.
-- The schema of the visit database is defined by HISTORY_VISIT_ROW_FIELDS
-- found in visit_database.h and is equivalent to:
--
-- CREATE TABLE visits(id INTEGER PRIMARY KEY AUTOINCREMENT,
--                     url_id INTEGER NOT NULL,
--                     visit_time INTEGER NOT NULL,
--                     from_visit INTEGER,
--                     transition INTEGER DEFAULT 0 NOT NULL,
--                     segment_id INTEGER,
--                     visit_duration INTEGER DEFAULT 0 NOT NULL,
--                     incremented_omnibox_typed_score BOOLEAN DEFAULT FALSE NOT NULL,
--                     opener_visit INTEGER,
--                     originator_cache_guid TEXT,
--                     originator_visit_id INTEGER,
--                     originator_from_visit INTEGER,
--                     originator_opener_visit INTEGER,
--                     is_known_to_sync INTEGER,
--                     consider_for_ntp_most_visited INTEGER,
--                     visited_link_id INTEGER,
--                     app_id TEXT)
INSERT INTO "urls" VALUES(1,'http://www.reuters.com/article/idUSN0839880620100708','UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters',3,1,2,0);  -- Qualifies
INSERT INTO "urls" VALUES(2,'http://www.golfweek.com/news/2010/jul/08/goydos-opens-john-deere-classic-59/','Goydos opens John Deere Classic with 59',3,1,4,0);  -- Qualifies
INSERT INTO "urls" VALUES(3,'http://www.businessandmedia.org/articles/2010/20100708120415.aspx','LeBronomics: Could High Taxes Influence James'' Team Decision?',4,1,2,0);  -- Qualifies
INSERT INTO "urls" VALUES(4,'http://www.realclearmarkets.com/articles/2010/07/08/diversity_in_the_financial_sector_98562.html','RealClearMarkets - Racial, Gender Quotas in the Financial Bill?',4,1,4,0);  -- Qualifies
INSERT INTO "urls" VALUES(5,'http://drudgereport.com/','DRUDGE REPORT 2010',3,2,2,0);  -- Qualifies
INSERT INTO "urls" VALUES(6,'http://totalfinder.binaryage.com/','TotalFinder brings tabs to your native Finder and more!',3,2,4,0);  -- Qualifies
INSERT INTO "urls" VALUES(7,'http://getsharekit.com/','ShareKit : Drop-in Share Features for all iOS Apps',4,2,4,0);  -- Qualifies
INSERT INTO "urls" VALUES(8,'http://getsharekit.com/index.html','ShareKit : Drop-in Share Features for all iOS Apps',3,0,4,0);
INSERT INTO "urls" VALUES(9,'http://en.wikipedia.org/wiki/Control-Z','Control-Z - Wikipedia, the free encyclopedia',0,0,6,0);
INSERT INTO "urls" VALUES(10,'http://vmware.com/info?id=724','VMware Account Management Login',1,0,6,0);
INSERT INTO "urls" VALUES(11,'http://www.tech-recipes.com/rx/2621/os_x_change_path_environment_variable/','OS X: Change your PATH environment variable | Mac system administration | Tech-Recipes',0,1,6,0);  -- Qualifies
INSERT INTO "urls" VALUES(12,'http://view.atdmt.com/PPJ/iview/194841301/direct;wi.160;hi.600/01?click=','',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(15,'http://www.cnn.com/','CNN.com International - Breaking, World, Business, Sports, Entertainment and Video News',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(16,'http://www.zdnet.com/','Technology News, Analysis, Comments and Product Reviews for IT Professionals | ZDNet',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(17,'http://www.crash.net/','Crash.Net | Formula 1 & MotoGP | Motorsport News',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(18,'http://www.theinquirer.net/','THE INQUIRER - Microprocessor, Server, Memory, PCS, Graphics, Networking, Storage',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(19,'http://www.theregister.co.uk/','The Register: Sci/Tech News for the World',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(20,'http://blogs.technet.com/markrussinovich/','Mark''s Blog - Site Home - TechNet Blogs',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(21,'http://www.icu-project.org/','ICU Home Page (ICU - International Components for Unicode)',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(22,'http://site.icu-project.org/','ICU Home Page (ICU - International Components for Unicode)',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(23,'http://icu-project.org/apiref/icu4c/','ICU 4.2: Main Page',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(24,'http://www.danilatos.com/event-test/ExperimentTest.html','Experimentation Harness',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(25,'http://www.codeguru.com/','CodeGuru : codeguru',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(26,'http://www.codeproject.com/','Your Development Resource - CodeProject',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(27,'http://www.tomshardware.com/us/#redir','Tom''s Hardware: Hardware News, Tests and Reviews',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(28,'http://www.ddj.com/windows/184416623','Dr. ABRACADABRA''s | Avoiding the Visual C++ Runtime Library | 2 1, 2003',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(29,'http://svcs.cnn.com/weather/getForecast?time=34&mode=json_html&zipCode=336736767676&locCode=EGLL&celcius=true&csiID=csi2','',6,6,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(30,'http://www.drudgery.com/Dogs%20and%20Mice','Life in the Slow Lane',8,2,2,0);  -- Qualifies
INSERT INTO "urls" VALUES(31,'http://www.redrudgerydo.com/','Music of the Wild Landscape',0,0,6,0);
INSERT INTO "urls" VALUES(32,'https://NearlyPerfectResult.com/','Practically Perfect Search Result',99,99,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(33,'http://QuiteUselessSearchResultxyz.com/','Practically Useless Search Result',4,0,99,0);  -- Qualifies
INSERT INTO "urls" VALUES(34,'http://FubarFubarAndFubar.com/','Situation Normal -- FUBARED',99,99,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(35,'http://en.wikipedia.org/wiki/1%25_rule_(Internet_culture)','Do Not Need Title',2,2,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(36,'http://default-engine.com?q=query','Query Query Query',2,2,0,0);  -- Qualifies
INSERT INTO "urls" VALUES(37,'http://view.atdmt.com/PPJ/iview/194841301/hidden/direct;wi.160;hi.600/01?click=','',6,6,0,1);
INSERT INTO "urls" VALUES(38,'http://svcs.cnn.com/hidden/weather/getForecast?time=34&mode=json_html&zipCode=336736767676&locCode=EGLL&celcius=true&csiID=csi2','',6,6,0,1);

-- This file creates some visits, enough to test (in InMemoryURLIndexTest)
-- the visits functionality, certainly not as many visits as are implied
-- by the visit counts associated with the URLs above.
INSERT INTO "visits" VALUES(1, 1, 2, 4, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(2, 1, 5, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(3, 1, 12, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(4, 32, 1, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(5, 32, 2, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(6, 32, 3, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(7, 32, 4, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(8, 32, 5, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(9, 32, 6, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(10, 32, 7, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(11, 32, 8, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(12, 32, 9, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(14, 32, 11, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(13, 32, 10, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(15, 32, 12, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(16, 32, 13, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(17, 32, 14, 0, '', 0, 0, 1, FALSE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(18, 32, 15, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(19, 35, 0, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(20, 35, 7, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(21, 36, 1, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
INSERT INTO "visits" VALUES(22, 36, 2, 0, '', 1, 0, 1, TRUE, 0, '', 0, 0, 0, FALSE, FALSE, 0, '');
