PRAGMA foreign_keys=1;
BEGIN TRANSACTION;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('last_compatible_version','1');
INSERT INTO "meta" VALUES('version','4');

CREATE TABLE eq_classes(
  id INTEGER,
  last_update_time INTEGER,
  group_display_name VARCHAR,
  group_icon_url VARCHAR,
  PRIMARY KEY(id));

CREATE TABLE eq_class_members(
  id INTEGER,
  facet_uri LONGVARCHAR NOT NULL,
  facet_display_name VARCHAR,
  facet_icon_url VARCHAR,
  set_id INTEGER NOT NULL REFERENCES eq_classes(id) ON DELETE CASCADE,
  PRIMARY KEY(id), UNIQUE(facet_uri));

CREATE TABLE eq_class_groups(
  id INTEGER,
  facet_uri LONGVARCHAR NOT NULL,
  set_id INTEGER NOT NULL REFERENCES eq_classes(id) ON DELETE CASCADE,
  main_domain VARCHAR,
  PRIMARY KEY(id));

CREATE INDEX index_on_eq_class_members_set_id ON eq_class_members (set_id);

INSERT INTO eq_classes(id, last_update_time, group_display_name, group_icon_url) VALUES (1, 1000000, 'Example.com', 'https://example.com/icon.png');
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://alpha.example.com', 1);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://beta.example.com', 1);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://gamma.example.com', 1);
INSERT INTO eq_class_groups(facet_uri, set_id, main_domain) VALUES ('https://alpha.example.com', 1, 'example.com');
INSERT INTO eq_class_groups(facet_uri, set_id, main_domain) VALUES ('https://beta.example.com', 1, 'example.com');
INSERT INTO eq_class_groups(facet_uri, set_id, main_domain) VALUES ('https://gamma.example.com', 1, 'example.com');

INSERT INTO eq_classes(id, last_update_time) VALUES (2, 2000000);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://delta.example.com', 2);
INSERT INTO eq_class_members(facet_uri, set_id) VALUES ('https://epsilon.example.com', 2);
INSERT INTO eq_class_groups(facet_uri, set_id) VALUES ('https://epsilon.example.com', 2);
INSERT INTO eq_class_groups(facet_uri, set_id) VALUES ('https://delta.example.com', 2);
INSERT INTO eq_class_groups(facet_uri, set_id) VALUES ('https://theta.example.com', 2);

INSERT INTO eq_classes(id, last_update_time, group_display_name, group_icon_url) VALUES (3, 3000000, 'Test Android App', 'https://example.com/icon.png');
INSERT INTO eq_class_members(facet_uri, facet_display_name, facet_icon_url, set_id) VALUES (
  'android://hash@com.example.android', 'Test Android App', 'https://example.com/icon.png', 3);
INSERT INTO eq_class_groups(facet_uri, set_id) VALUES (
  'android://hash@com.example.android', 3);

COMMIT;
