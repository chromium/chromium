// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/lazy_leveldb.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace value_store {

namespace {

const char kInvalidJson[] = "Invalid JSON";
const char kRestoredDuringOpen[] = "Database corruption repaired during open";

// UMA values used when recovering from a corrupted leveldb.
// Do not change/delete these values as you will break reporting for older
// copies of Chrome. Only add new values to the end.
enum LevelDBDatabaseCorruptionRecoveryValue {
  LEVELDB_DB_RESTORE_DELETE_SUCCESS = 0,
  LEVELDB_DB_RESTORE_DELETE_FAILURE,
  LEVELDB_DB_RESTORE_REPAIR_SUCCESS,
  LEVELDB_DB_RESTORE_MAX
};

// UMA values used when recovering from a corrupted leveldb.
// Do not change/delete these values as you will break reporting for older
// copies of Chrome. Only add new values to the end.
enum LevelDBValueCorruptionRecoveryValue {
  LEVELDB_VALUE_RESTORE_DELETE_SUCCESS,
  LEVELDB_VALUE_RESTORE_DELETE_FAILURE,
  LEVELDB_VALUE_RESTORE_MAX
};

ValueStore::StatusCode LevelDbToValueStoreStatusCode(
    const leveldb::Status& status) {
  if (status.ok())
    return ValueStore::OK;
  if (status.IsCorruption())
    return ValueStore::CORRUPTION;
  return ValueStore::OTHER_ERROR;
}

leveldb::Status DeleteValue(leveldb::DB* db, const std::string& key) {
  leveldb::WriteBatch batch;
  batch.Delete(key);

  return db->Write(leveldb::WriteOptions(), &batch);
}

}  // namespace

LazyLevelDb::LazyLevelDb(const std::string& uma_client_name,
                         const base::FilePath& path)
    : db_path_(path) {
  open_options_.create_if_missing = true;
  open_options_.paranoid_checks = true;

  read_options_.verify_checksums = true;

  // TODO(crbug.com/40189032): Remove reference to extensions.
  // Used in lieu of UMA_HISTOGRAM_ENUMERATION because the histogram name is
  // not a constant.
  open_histogram_ = base::LinearHistogram::FactoryGet(
      "Extensions.Database.Open." + uma_client_name, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
}

LazyLevelDb::~LazyLevelDb() = default;

ValueStore::Status LazyLevelDb::Read(const std::string& key,
                                     std::optional<base::Value>* value) {
  DCHECK(value);

  std::string value_as_json;
  leveldb::Status s = db_->Get(read_options_, key, &value_as_json);

  if (s.IsNotFound()) {
    // Despite there being no value, it was still a success. Check this first
    // because ok() is false on IsNotFound.
    return ValueStore::Status();
  }

  if (!s.ok())
    return ToValueStoreError(s);

  std::optional<base::Value> read_value = base::JSONReader::Read(value_as_json);
  if (!read_value) {
    return ValueStore::Status(ValueStore::CORRUPTION, FixCorruption(&key),
                              kInvalidJson);
  }
  *value = std::move(read_value);
  return ValueStore::Status();
}

ValueStore::Status LazyLevelDb::Delete(const std::string& key) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;

  return ToValueStoreError(DeleteValue(db_.get(), key));
}

ValueStore::BackingStoreRestoreStatus LazyLevelDb::FixCorruption(
    const std::string* key) {
  leveldb::Status s;
  if (key && db_) {
    s = DeleteValue(db_.get(), *key);
    // Deleting involves writing to the log, so it's possible to have a
    // perfectly OK database but still have a delete fail.
    if (s.ok())
      return ValueStore::VALUE_RESTORE_DELETE_SUCCESS;
    else if (s.IsIOError())
      return ValueStore::VALUE_RESTORE_DELETE_FAILURE;
    // Any other kind of failure triggers a db repair.
  }

  // Make sure database is closed.
  db_.reset();

  // First try the less lossy repair.
  ValueStore::BackingStoreRestoreStatus restore_status =
      ValueStore::RESTORE_NONE;

  leveldb_env::Options repair_options;
  repair_options.reuse_logs = false;
  repair_options.create_if_missing = true;
  repair_options.paranoid_checks = true;

  // RepairDB can drop an unbounded number of leveldb tables (key/value sets).
  s = leveldb::RepairDB(db_path_.AsUTF8Unsafe(), repair_options);

  if (s.ok()) {
    restore_status = ValueStore::DB_RESTORE_REPAIR_SUCCESS;
    s = leveldb_env::OpenDB(open_options_, db_path_.AsUTF8Unsafe(), &db_);
  }

  if (!s.ok()) {
    if (DeleteDbFile()) {
      restore_status = ValueStore::DB_RESTORE_DELETE_SUCCESS;
      s = leveldb_env::OpenDB(open_options_, db_path_.AsUTF8Unsafe(), &db_);
    } else {
      restore_status = ValueStore::DB_RESTORE_DELETE_FAILURE;
    }
  }

  if (!s.ok())
    db_unrecoverable_ = true;

  if (s.ok() && key) {
    s = DeleteValue(db_.get(), *key);
    if (s.ok()) {
      restore_status = ValueStore::VALUE_RESTORE_DELETE_SUCCESS;
    } else if (s.IsIOError()) {
      restore_status = ValueStore::VALUE_RESTORE_DELETE_FAILURE;
    } else {
      db_.reset();
      if (!DeleteDbFile())
        db_unrecoverable_ = true;
      restore_status = ValueStore::DB_RESTORE_DELETE_FAILURE;
    }
  }

  return restore_status;
}

ValueStore::Status LazyLevelDb::EnsureDbIsOpen() {
  if (db_)
    return ValueStore::Status();

  if (db_unrecoverable_) {
    return ValueStore::Status(ValueStore::CORRUPTION,
                              ValueStore::DB_RESTORE_DELETE_FAILURE,
                              "Database corrupted");
  }

  leveldb::Status ldb_status =
      leveldb_env::OpenDB(open_options_, db_path_.AsUTF8Unsafe(), &db_);
  open_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(ldb_status));
  ValueStore::Status status = ToValueStoreError(ldb_status);
  if (ldb_status.IsCorruption()) {
    status.restore_status = FixCorruption(nullptr);
    if (status.restore_status != ValueStore::DB_RESTORE_DELETE_FAILURE) {
      status.code = ValueStore::OK;
      status.message = kRestoredDuringOpen;
    }
  }

  return status;
}

ValueStore::Status LazyLevelDb::ToValueStoreError(
    const leveldb::Status& status) {
  if (status.ok())
    return ValueStore::Status();

  CHECK(!status.IsNotFound());  // not an error

  std::string message = status.ToString();
  // The message may contain |db_path_|, which may be considered sensitive
  // data, and those strings are passed to the extension, so strip it out.
  base::ReplaceSubstringsAfterOffset(&message, 0u, db_path_.AsUTF8Unsafe(),
                                     "...");

  return ValueStore::Status(LevelDbToValueStoreStatusCode(status), message);
}

bool LazyLevelDb::DeleteDbFile() {
  db_.reset();  // Close the database.

  leveldb::Status s =
      leveldb::DestroyDB(db_path_.AsUTF8Unsafe(), leveldb_env::Options());
  if (!s.ok()) {
    LOG(WARNING) << "Failed to destroy leveldb database at "
                 << db_path_.value();
    return false;
  }
  return true;
}

ValueStore::Status LazyLevelDb::CreateIterator(
    const leveldb::ReadOptions& read_options,
    std::unique_ptr<leveldb::Iterator>* iterator) {
  ValueStore::Status status = EnsureDbIsOpen();
  if (!status.ok())
    return status;
  *iterator = base::WrapUnique(db_->NewIterator(read_options));
  return ValueStore::Status();
}

}  // namespace value_store
