// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_LAZY_LEVELDB_H_
#define COMPONENTS_VALUE_STORE_LAZY_LEVELDB_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/values.h"
#include "components/value_store/value_store.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {
class Iterator;
}  // namespace leveldb

namespace value_store {

// Manages a lazy connection to a leveldb database. "lazy" means that the
// leveldb database will be opened, when necessary, when any *public* method is
// called. Derived classes are responsible for calling EnsureDbIsOpen() before
// calling any other *protected* method.
class LazyLevelDb {
 public:
  LazyLevelDb(const LazyLevelDb&) = delete;
  LazyLevelDb& operator=(const LazyLevelDb&) = delete;

  // Creates a new database iterator. This iterator *must* be deleted before
  // this database is closed.
  ValueStore::Status CreateIterator(
      const leveldb::ReadOptions& read_options,
      std::unique_ptr<leveldb::Iterator>* iterator);

  // Converts a leveldb::Status to a ValueStore::Status. Will also sanitize path
  // to eliminate user data path.
  ValueStore::Status ToValueStoreError(const leveldb::Status& status);

  // Deletes a value (identified by |key|) from the database.
  ValueStore::Status Delete(const std::string& key);

 protected:
  LazyLevelDb(const std::string& uma_client_name, const base::FilePath& path);
  ~LazyLevelDb();

  // Closes, if necessary, and deletes the database directory.
  bool DeleteDbFile();

  // Fixes the |key| or database. If |key| is not null and the database is open
  // then the key will be deleted. Otherwise the database will be repaired, and
  // failing that will be deleted.
  ValueStore::BackingStoreRestoreStatus FixCorruption(const std::string* key);

  // Reads a |key| from the database, and populates |value| with the result. If
  // the specified value does not exist in the database then an "OK" status will
  // be returned and value will be unchanged. Caller must ensure the database is
  // open before calling this method.
  ValueStore::Status Read(const std::string& key,
                          std::optional<base::Value>* value);

  // Opens the underlying database if not yet open. If the open fails due to
  // corruption will attempt to repair the database. Failing that, will attempt
  // to delete the database. Will only attempt a single recovery.
  ValueStore::Status EnsureDbIsOpen();

  const char* open_histogram_name() const {
    return open_histogram_->histogram_name();
  }

  leveldb::DB* db() { return db_.get(); }

  const leveldb::ReadOptions& read_options() const { return read_options_; }

  const leveldb::WriteOptions& write_options() const { return write_options_; }

 private:
  // The leveldb to which this class reads/writes.
  std::unique_ptr<leveldb::DB> db_;
  // The path to the underlying leveldb.
  const base::FilePath db_path_;
  // The options to be used when this database is lazily opened.
  leveldb_env::Options open_options_;
  // The options to be used for all database read operations.
  leveldb::ReadOptions read_options_;
  // The options to be used for all database write operations.
  leveldb::WriteOptions write_options_;
  // Set when this database has tried to repair (and failed) to prevent
  // unbounded attempts to open a bad/unrecoverable database.
  bool db_unrecoverable_ = false;
  // Used for UMA logging.
  raw_ptr<base::HistogramBase> open_histogram_ = nullptr;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_LAZY_LEVELDB_H_
