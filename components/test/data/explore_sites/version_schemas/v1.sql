INSERT OR REPLACE INTO meta (key, value)
VALUES ('version', 1), ('last_compatible_version', 1);

CREATE TABLE IF NOT EXISTS categories (
category_id INTEGER PRIMARY KEY AUTOINCREMENT,
version_token TEXT NOT NULL,
type INTEGER NOT NULL,
label TEXT NOT NULL,
image BLOB,
ntp_click_count INTEGER NOT NULL DEFAULT 0,
esp_site_click_count INTEGER NOT NULL DEFAULT 0);

INSERT INTO categories
(version_token, type, label)
VALUES ('versionToken', 3, 'category1');

CREATE TABLE IF NOT EXISTS sites (
site_id INTEGER PRIMARY KEY AUTOINCREMENT,
url TEXT NOT NULL,
category_id INTEGER NOT NULL,
title TEXT NOT NULL,
favicon BLOB,
click_count INTEGER NOT NULL DEFAULT 0,
removed BOOLEAN NOT NULL default FALSE);

INSERT INTO sites
(url, category_id, title)
VALUES ('http://www.google.com', 1, 'site1');

CREATE TABLE IF NOT EXISTS activity (
time INTEGER NOT NULL,
category_type INTEGER NOT NULL,
url TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS site_blacklist
(url TEXT NOT NULL UNIQUE,
date_removed INTEGER NOT NULL);

INSERT INTO site_blacklist
(url, date_removed)
VALUES ('http://www.example.com', 1);
