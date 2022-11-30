INSERT OR REPLACE INTO meta (key, value)
VALUES ('version', 1), ('last_compatible_version', 1);

CREATE TABLE IF NOT EXISTS prefetch_items
(
offline_id INTEGER PRIMARY KEY NOT NULL,
state INTEGER NOT NULL DEFAULT 0,
generate_bundle_attempts INTEGER NOT NULL DEFAULT 0,
get_operation_attempts INTEGER NOT NULL DEFAULT 0,
download_initiation_attempts INTEGER NOT NULL DEFAULT 0,
archive_body_length INTEGER_NOT_NULL DEFAULT -1,
creation_time INTEGER NOT NULL,
freshness_time INTEGER NOT NULL,
error_code INTEGER NOT NULL DEFAULT 0,
file_size INTEGER NOT NULL DEFAULT 0,
guid VARCHAR NOT NULL DEFAULT '',
client_namespace VARCHAR NOT NULL DEFAULT '',
client_id VARCHAR NOT NULL DEFAULT '',
requested_url VARCHAR NOT NULL DEFAULT '',
final_archived_url VARCHAR NOT NULL DEFAULT '',
operation_name VARCHAR NOT NULL DEFAULT '',
archive_body_name VARCHAR NOT NULL DEFAULT '',
title VARCHAR NOT NULL DEFAULT '',
file_path VARCHAR NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS prefetch_downloader_quota
(
quota_id INTEGER PRIMARY KEY NOT NULL DEFAULT 1,
update_time INTEGER NOT NULL,
available_quota INTEGER NOT NULL DEFAULT 0
);


INSERT INTO prefetch_items
(
offline_id,
state,
generate_bundle_attempts,
get_operation_attempts,
download_initiation_attempts,
archive_body_length,
creation_time,
freshness_time,
error_code,
file_size,
guid,
client_namespace,
client_id,
requested_url,
final_archived_url,
operation_name,
archive_body_name,
title,
file_path
)
VALUES
(
1,  -- offline_id
2,  -- state
3,  -- generate_bundle_attempts
4,  -- get_operation_attempts
5,  -- download_initiation_attempts
6,  -- archive_body_length
7,  -- creation_time
8,  -- freshness_time
9,  -- error_code
10,  -- file_size
'guid',  -- guid
'client_namespace',  -- client_namespace
'client_id',  -- client_id
'requested_url',  -- requested_url
'final_archived_url',  -- final_archived_url
'operation_name',  -- operation_name
'archive_body_name',  -- archive_body_name
'title',  -- title
'file_path'  -- file_path
);

INSERT INTO prefetch_downloader_quota
(
quota_id,
update_time,
available_quota
)
VALUES
(
1,
2,
3
);
