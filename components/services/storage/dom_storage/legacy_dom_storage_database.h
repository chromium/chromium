// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEGACY_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEGACY_DOM_STORAGE_DATABASE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "sql/database.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace storage {

class FilesystemProxy;

using LegacyDomStorageValuesMap =
    std::map<std::u16string, base::Optional<std::u16string>>;

// Represents a SQLite based backing for DOM storage data. This
// class is designed to be used on a single thread.
class LegacyDomStorageDatabase {
 public:
  LegacyDomStorageDatabase(const base::FilePath& file_path,
                           std::unique_ptr<FilesystemProxy> filesystem_proxy);
  virtual ~LegacyDomStorageDatabase();  // virtual for unit testing

  // Reads all the key, value pairs stored in the database and returns them.
  // |result| is assumed to be empty and any duplicate keys will be overwritten.
  // If the database exists on disk then it will be opened. If it does not exist
  // then it will not be created and |result| will be unmodified.
  void ReadAllValues(LegacyDomStorageValuesMap* result);

  // Updates the backing database. Will remove all keys before updating the
  // database if |clear_all_first| is set. Then all entries in |changes| will be
  // examined - keys mapped to a nullopt value will be removed and all others
  // will be inserted/updated as appropriate.
  bool CommitChanges(bool clear_all_first,
                     const LegacyDomStorageValuesMap& changes);

  // Adds memory statistics of the database to |pmd| for tracing.
  void ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                         const std::string& name);

  // Simple getter for the path we were constructed with.
  const base::FilePath& file_path() const { return file_path_; }

 protected:
  // Constructor that uses an in-memory sqlite database, for testing.
  explicit LegacyDomStorageDatabase(
      std::unique_ptr<FilesystemProxy> filesystem_proxy);

 private:
  friend class LocalStorageDatabaseAdapter;
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest, SimpleOpenAndClose);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest, TestLazyOpenIsLazy);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           TestDetectSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           TestLazyOpenUpgradesDatabase);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           SimpleWriteAndReadBack);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest, WriteWithClear);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           UpgradeFromV1ToV2WithData);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           TestSimpleRemoveOneValue);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           TestCanOpenAndReadWebCoreDatabase);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageDatabaseTest,
                           TestCanOpenFileThatIsNotADatabase);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageAreaParamTest,
                           ShallowCopyWithBacking);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageAreaTest,
                           EnableDisableCachingWithBacking);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(LegacyDomStorageAreaTest, PurgeMemory);

  enum SchemaVersion {
    INVALID,

    // V1 is deprecated.

    // 2011-07-15 - https://bugs.webkit.org/show_bug.cgi?id=58762
    V2,
  };

  // Open the database at file_path_ if it exists already and creates it if
  // |create_if_needed| is true.
  // Ensures we are at the correct database version and creates or updates
  // tables as necessary. Returns false on failure.
  bool LazyOpen(bool create_if_needed);

  // Analyses the database to verify that the connection that is open is indeed
  // a valid database and works out the schema version.
  SchemaVersion DetectSchemaVersion();

  // Creates the database table at V2. Returns true if the table was created
  // successfully, false otherwise. Will return false if the table already
  // exists.
  bool CreateTableV2();

  // If we have issues while trying to open the file (corrupted databse,
  // failing to upgrade, that sort of thing) this function will remove
  // the file from disk and attempt to create a new database from
  // scratch.
  bool DeleteFileAndRecreate();

  void Close();
  bool IsOpen() const { return db_.get() ? db_->is_open() : false; }

  // Initialization code shared between the two constructors of this class.
  void Init();

  // Path to the database on disk.
  const base::FilePath file_path_;
  const std::unique_ptr<FilesystemProxy> filesystem_proxy_;
  std::unique_ptr<sql::Database> db_;
  bool failed_to_open_;
  bool tried_to_recreate_;
  bool known_to_be_empty_;

  DISALLOW_COPY_AND_ASSIGN(LegacyDomStorageDatabase);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEGACY_DOM_STORAGE_DATABASE_H_
