// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_database.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace media_device_salt {

namespace {
// The current version of the database schema.
constexpr int kCurrentVersion = 1;

// The lowest version of the database schema such that old versions of the code
// can still read/write the current database.
constexpr int kCompatibleVersion = 1;
}  // namespace

std::string CreateRandomSalt() {
  return base::UnguessableToken::Create().ToString();
}

MediaDeviceSaltDatabase::MediaDeviceSaltDatabase(const base::FilePath& db_path)
    : db_path_(db_path),
      db_(sql::DatabaseOptions{.page_size = 4096, .cache_size = 16}) {}

std::optional<std::string> MediaDeviceSaltDatabase::GetOrInsertSalt(
    const blink::StorageKey& storage_key,
    std::optional<std::string> candidate_salt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_key.origin().opaque() || !EnsureOpen()) {
    return std::nullopt;
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return std::nullopt;
  }
  static constexpr char kGetSaltSql[] =
      "SELECT salt FROM media_device_salts WHERE storage_key=?";
  DCHECK(db_.IsSQLValid(kGetSaltSql));
  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetSaltSql));
  select_statement.BindString(0, storage_key.Serialize());
  if (select_statement.Step()) {
    return select_statement.ColumnString(0);
  }
  if (!select_statement.Succeeded()) {
    return std::nullopt;
  }

  static constexpr char kInsertSaltSql[] =
      "INSERT INTO media_device_salts(storage_key,creation_time,salt) "
      "VALUES(?,?,?)";
  DCHECK(db_.IsSQLValid(kInsertSaltSql));
  sql::Statement insert_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertSaltSql));
  insert_statement.BindString(0, storage_key.Serialize());
  insert_statement.BindTime(1, base::Time::Now());
  std::string new_salt = candidate_salt.value_or(CreateRandomSalt());
  insert_statement.BindString(2, new_salt);
  return insert_statement.Run() && transaction.Commit()
             ? std::make_optional(new_salt)
             : std::nullopt;
}

void MediaDeviceSaltDatabase::DeleteEntries(
    base::Time delete_begin,
    base::Time delete_end,
    content::StoragePartition::StorageKeyMatcherFunction matcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureOpen()) {
    return;
  }

  if (matcher.is_null()) {
    static constexpr char kDeleteSaltsSql[] =
        "DELETE FROM media_device_salts "
        "WHERE creation_time>=? AND creation_time<=?";
    DCHECK(db_.IsSQLValid(kDeleteSaltsSql));
    sql::Statement statement(db_.GetUniqueStatement(kDeleteSaltsSql));
    statement.BindTime(0, delete_begin);
    statement.BindTime(1, delete_end);
    statement.Run();
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return;
  }
  static constexpr char kGetStorageKeysSql[] =
      "SELECT storage_key "
      "FROM media_device_salts "
      "WHERE creation_time>=? AND creation_time<=?";
  DCHECK(db_.IsSQLValid(kGetStorageKeysSql));
  sql::Statement select_statement(db_.GetUniqueStatement(kGetStorageKeysSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  std::vector<std::string> serialized_storage_keys;
  while (select_statement.Step()) {
    serialized_storage_keys.push_back(select_statement.ColumnString(0));
  }

  std::erase_if(serialized_storage_keys,
                [&matcher](const std::string& serialized_storage_key) {
                  std::optional<blink::StorageKey> storage_key =
                      blink::StorageKey::Deserialize(serialized_storage_key);
                  if (!storage_key.has_value()) {
                    // This shouldn't happen, but include non-Deserializable
                    // keys for deletion if they're found.
                    return false;
                  }
                  return !matcher.Run(*storage_key);
                });

  if (serialized_storage_keys.empty()) {
    return;
  }

  const std::string delete_storage_keys_sql =
      base::StrCat({"DELETE FROM media_device_salts "
                    "WHERE storage_key IN ('",
                    base::JoinString(serialized_storage_keys, "','"), "')"});
  DCHECK(db_.IsSQLValid(delete_storage_keys_sql));
  sql::Statement delete_statement(
      db_.GetUniqueStatement(delete_storage_keys_sql));
  delete_statement.Run() && transaction.Commit();
}

void MediaDeviceSaltDatabase::DeleteEntry(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_key.origin().opaque() || !EnsureOpen()) {
    return;
  }
  static constexpr char kDeleteStorageKeySql[] =
      "DELETE FROM media_device_salts "
      "WHERE storage_key=?";
  DCHECK(db_.IsSQLValid(kDeleteStorageKeySql));
  sql::Statement statement(db_.GetUniqueStatement(kDeleteStorageKeySql));
  statement.BindString(0, storage_key.Serialize());
  statement.Run();
}

std::vector<blink::StorageKey> MediaDeviceSaltDatabase::GetAllStorageKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureOpen()) {
    return {};
  }
  std::vector<blink::StorageKey> storage_keys;
  static constexpr char kGetStorageKeysSql[] =
      "SELECT storage_key FROM media_device_salts";
  DCHECK(db_.IsSQLValid(kGetStorageKeysSql));
  sql::Statement statement(db_.GetUniqueStatement(kGetStorageKeysSql));
  while (statement.Step()) {
    std::optional<blink::StorageKey> key =
        blink::StorageKey::Deserialize(statement.ColumnString(0));
    if (key.has_value()) {
      storage_keys.push_back(*key);
    }
  }
  return storage_keys;
}

bool MediaDeviceSaltDatabase::EnsureOpen(bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return true;
  }
  if (force_error_) {
    return false;
  }

  db_.set_histogram_tag("MediaDeviceSalts");
  // base::Unretained() is safe here because `this` owns `db`.
  db_.set_error_callback(base::BindRepeating(
      &MediaDeviceSaltDatabase::OnDatabaseError, base::Unretained(this)));
  if (db_path_.empty()) {
    if (!db_.OpenInMemory()) {
      return false;
    }
  } else if (!db_.Open(db_path_)) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (transaction.Begin()) {
    sql::MetaTable metatable;
    if (metatable.Init(&db_, kCurrentVersion, kCompatibleVersion) &&
        metatable.GetCompatibleVersionNumber() <= kCurrentVersion &&
        db_.Execute("CREATE TABLE IF NOT EXISTS media_device_salts("
                    "  storage_key TEXT PRIMARY KEY NOT NULL,"
                    "  creation_time INTEGER NOT NULL,"
                    "  salt TEXT NOT NULL)") &&
        db_.Execute("CREATE INDEX IF NOT EXISTS creation_time ON "
                    "media_device_salts(creation_time)") &&
        transaction.Commit()) {
      return true;
    }
    transaction.Rollback();
  }

  db_.Raze();
  return is_retry ? false : EnsureOpen(/*is_retry=*/true);
}

void MediaDeviceSaltDatabase::OnDatabaseError(int error,
                                              sql::Statement* statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("Media.MediaDevices.SaltDatabaseErrors", error);
  std::ignore = sql::Recovery::RecoverIfPossible(
      &db_, error, sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze);
}

}  // namespace media_device_salt
