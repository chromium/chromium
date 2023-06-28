PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE environment_integrity_handles (
origin TEXT NOT NULL,
handle INTEGER NOT NULL,
PRIMARY KEY (origin));

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','0');
INSERT INTO meta VALUES('last_compatible_version','1');

INSERT INTO environment_integrity_handles VALUES ('https://foo.com', 123);

COMMIT;
