BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('last_compatible_version','31');
INSERT INTO "meta" VALUES('version','32');
CREATE TABLE logins (
origin_url VARCHAR NOT NULL,
action_url VARCHAR,
username_element VARCHAR,
username_value VARCHAR,
password_element VARCHAR,
password_value BLOB,
submit_element VARCHAR,
signon_realm VARCHAR NOT NULL,
date_created INTEGER NOT NULL,
blacklisted_by_user INTEGER NOT NULL,
scheme INTEGER NOT NULL,
password_type INTEGER,
times_used INTEGER,
form_data BLOB,
display_name VARCHAR,
icon_url VARCHAR,
federation_url VARCHAR,
skip_zero_click INTEGER,
generation_upload_status INTEGER,
possible_username_pairs BLOB,
id INTEGER PRIMARY KEY AUTOINCREMENT,
date_last_used INTEGER,
moving_blocked_for BLOB,
date_password_modified INTEGER,
UNIQUE (origin_url, username_element, username_value, password_element, signon_realm));
INSERT INTO "logins" (origin_url,action_url,username_element,username_value,password_element,password_value,submit_element,signon_realm,date_created,blacklisted_by_user,scheme,password_type,times_used,form_data,display_name,icon_url,federation_url,skip_zero_click,generation_upload_status,possible_username_pairs,date_last_used,moving_blocked_for,date_password_modified) VALUES(
'https://accounts.google.com/ServiceLogin', /* origin_url */
'https://accounts.google.com/ServiceLoginAuth', /* action_url */
'Email', /* username_element */
'theerikchen', /* username_value */
'Passwd', /* password_element */
X'', /* password_value */
'', /* submit_element */
'https://accounts.google.com/', /* signon_realm */
13047429345000000, /* date_created */
0, /* blacklisted_by_user */
0, /* scheme */
0, /* password_type */
1, /* times_used */
X'18000000020000000000000000000000000000000000000000000000', /* form_data */
'', /* display_name */
'', /* icon_url */
'', /* federation_url */
1,  /* skip_zero_click */
0,  /* generation_upload_status */
X'00000000', /* possible_username_pairs */
0, /* date_last_used */
X'', /* moving_blocked_for */
0 /* date_password_modified */
);
INSERT INTO "logins" (origin_url,action_url,username_element,username_value,password_element,password_value,submit_element,signon_realm,date_created,blacklisted_by_user,scheme,password_type,times_used,form_data,display_name,icon_url,federation_url,skip_zero_click,generation_upload_status,possible_username_pairs,date_last_used,moving_blocked_for,date_password_modified) VALUES(
'https://accounts.google.com/ServiceLogin', /* origin_url */
'https://accounts.google.com/ServiceLoginAuth', /* action_url */
'Email', /* username_element */
'theerikchen2', /* username_value */
'Passwd', /* password_element */
X'', /* password_value */
'non-empty', /* submit_element */
'https://accounts.google.com/', /* signon_realm */
13047423600000000, /* date_created */
0, /* blacklisted_by_user */
0, /* scheme */
0, /* password_type */
1, /* times_used */
X'18000000020000000000000000000000000000000000000000000000', /* form_data */
'', /* display_name */
'https://www.google.com/icon', /* icon_url */
'', /* federation_url */
1,  /* skip_zero_click */
0,  /* generation_upload_status */
X'00000000', /* possible_username_pairs */
0, /* date_last_used */
X'2400000020000000931DDD1C53CCD2CE11C30C3027798FF7E31CCFE83FB086F3A7797F404D647332', /* moving_blocked_for */
13047423610000000 /* date_password_modified */
);
INSERT INTO "logins" (origin_url,action_url,username_element,username_value,password_element,password_value,submit_element,signon_realm,date_created,blacklisted_by_user,scheme,password_type,times_used,form_data,display_name,icon_url,federation_url,skip_zero_click,generation_upload_status,possible_username_pairs,date_last_used,moving_blocked_for,date_password_modified) VALUES(
'http://example.com', /* origin_url */
'http://example.com/landing', /* action_url */
'', /* username_element */
'user', /* username_value */
'', /* password_element */
X'', /* password_value */
'non-empty', /* submit_element */
'http://example.com', /* signon_realm */
13047423600000000, /* date_created */
0, /* blacklisted_by_user */
1, /* scheme */
0, /* password_type */
1, /* times_used */
X'18000000020000000000000000000000000000000000000000000000', /* form_data */
'', /* display_name */
'https://www.google.com/icon', /* icon_url */
'', /* federation_url */
1,  /* skip_zero_click */
0,  /* generation_upload_status */
X'00000000', /* possible_username_pairs */
0, /* date_last_used */
X'', /* moving_blocked_for */
13047423610000000 /* date_password_modified */
);
CREATE INDEX logins_signon ON logins (signon_realm);
CREATE TABLE stats (
origin_domain VARCHAR NOT NULL,
username_value VARCHAR,
dismissal_count INTEGER,
update_time INTEGER NOT NULL,
UNIQUE(origin_domain, username_value));
CREATE INDEX stats_origin ON stats(origin_domain);
CREATE TABLE sync_entities_metadata (
  storage_key INTEGER PRIMARY KEY AUTOINCREMENT,
  metadata VARCHAR NOT NULL
);
CREATE TABLE sync_model_metadata (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  metadata VARCHAR NOT NULL
);
CREATE TABLE insecure_credentials (
parent_id INTEGER REFERENCES logins ON UPDATE CASCADE ON DELETE CASCADE
  DEFERRABLE INITIALLY DEFERRED,
insecurity_type INTEGER NOT NULL,
create_time INTEGER NOT NULL,
is_muted INTEGER NOT NULL DEFAULT 0,
UNIQUE (parent_id, insecurity_type));
CREATE INDEX foreign_key_index ON insecure_credentials (parent_id);
INSERT INTO "insecure_credentials"
  (parent_id,insecurity_type,create_time,is_muted) VALUES(
1, /* parent_id */
0, /* compromise_type */
13047423600000000, /* create_time */
0 /* is_muted */
);
INSERT INTO "insecure_credentials"
  (parent_id,insecurity_type,create_time,is_muted) VALUES(
1, /* parent_id */
1, /* compromise_type */
13047423600000000, /* create_time */
0 /* is_muted */
);
COMMIT;
