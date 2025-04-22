PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO meta VALUES('version','6');
INSERT INTO meta VALUES('last_compatible_version','5');

CREATE TABLE tokens (
version INTEGER NOT NULL,
u BLOB NOT NULL,
e BLOB NOT NULL,
epoch_id TEXT NOT NULL,
expiration INTEGER NOT NULL,
num_tokens_with_signal INTEGER NOT NULL,
public_key TEXT NOT NULL,
batch_size INTEGER NOT NULL
);

INSERT INTO tokens VALUES (1, 'u', 'e', '321', 123, 100, 'public_key', 1);

COMMIT;
