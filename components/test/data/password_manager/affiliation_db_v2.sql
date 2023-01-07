PRAGMA foreign_keys=1;
BEGIN TRANSACTION;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('last_compatible_version','1');
INSERT INTO "meta" VALUES('version','2');

CREATE TABLE eq_classes(
  id INTEGER,
  last_update_time INTEGER,
  PRIMARY KEY(id));

CREATE TABLE eq_class_members(
  id INTEGER,
  facet_uri LONGVARCHAR NOT NULL,
  facet_display_name VARCHAR,
  facet_icon_url VARCHAR,
  set_id INTEGER NOT NULL REFERENCES eq_classes(id) ON DELETE CASCADE,
  PRIMARY KEY(id), UNIQUE(facet_uri));

CREATE INDEX index_on_eq_class_members_set_id ON eq_class_members (set_id);

INSERT INTO eq_classes(id, last_update_time) VALUES (1, 1000000);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://alpha.example.com', 1);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://beta.example.com', 1);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://gamma.example.com', 1);

INSERT INTO eq_classes(id, last_update_time) VALUES (2, 2000000);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://delta.example.com', 2);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://epsilon.example.com', 2);

INSERT INTO eq_classes(id, last_update_time) VALUES (3, 3000000);
INSERT INTO eq_class_members(facet_uri, facet_display_name, facet_icon_url, set_id) VALUES (
  'android://hash@com.example.android', 'Test Android App', 'https://example.com/icon.png', 3);

COMMIT;
