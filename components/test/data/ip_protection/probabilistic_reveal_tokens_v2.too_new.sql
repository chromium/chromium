PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO meta VALUES('version','2');
INSERT INTO meta VALUES('last_compatible_version','1');

CREATE TABLE tokens (
version INTEGER NOT NULL,
u TEXT NOT NULL,
e TEXT NOT NULL,
expiration INTEGER NOT NULL,
num_tokens_with_signal INTEGER NOT NULL,
public_key TEXT NOT NULL
);

INSERT INTO tokens VALUES (1, 'u', 'e', 123, 100, 'public_key');

COMMIT;
