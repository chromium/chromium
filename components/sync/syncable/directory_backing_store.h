// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_
#define COMPONENTS_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/syncable/dir_open_result.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/metahandle_set.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {
namespace syncable {

extern const int32_t kCurrentDBVersion;
extern const int32_t kCurrentPageSizeKB;

// Interface that provides persistence for a syncable::Directory object. You can
// load all the persisted data to prime a syncable::Directory on startup by
// invoking Load.  The only other thing you (or more correctly, a Directory) can
// do here is save any changes that have occurred since calling Load, which can
// be done periodically as often as desired.
//
// The DirectoryBackingStore will own an sqlite lock on its database for most of
// its lifetime.  You must not have two DirectoryBackingStore objects accessing
// the database simultaneously.  Because the lock exists at the database level,
// not even two separate browser instances would be able to acquire it
// simultaneously.
//
// This class is abstract so that we can extend it in interesting ways for use
// in tests.  The concrete class used in non-test scenarios is
// OnDiskDirectoryBackingStore.
class DirectoryBackingStore {
 public:
  DirectoryBackingStore(
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator);
  virtual ~DirectoryBackingStore();

  // Loads and drops all currently persisted meta entries into |handles_map|
  // and loads appropriate persisted kernel info into |kernel_load_info|.
  // The function determines which entries can be safely dropped and inserts
  // their keys into |metahandles_to_purge|. It is up to the caller to
  // perform the actual cleanup.
  //
  // This function will migrate to the latest database version.
  //
  // NOTE: On success (return value of OPENED), the buckets are populated with
  // newly allocated items, meaning ownership is bestowed upon the caller.
  virtual DirOpenResult Load(Directory::MetahandlesMap* handles_map,
                             MetahandleSet* metahandles_to_purge,
                             Directory::KernelLoadInfo* kernel_load_info) = 0;

  // Updates the on-disk store with the input |snapshot| as a database
  // transaction.  Does NOT open any syncable transactions as this would cause
  // opening transactions elsewhere to block on synchronous I/O.
  // DO NOT CALL THIS FROM MORE THAN ONE THREAD EVER.  Also, whichever thread
  // calls SaveChanges *must* be the thread that owns/destroys |this|.
  //
  // Returns true if the changes were saved successfully. Returns false if an
  // error (of any kind) occurred. See also |SetCatastrophicErrorHandler| for a
  // systematic way of handling underlying DB errors.
  virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot);

  // Set the catastrophic error handler.
  //
  // When a catastrophic error is encountered while accessing the underlying DB,
  // |catastrophic_error_handler| will be invoked (via PostTask) on the thread
  // on which this DirectoryBackingStore object lives.
  //
  // For a definition of what's catastrophic, see sql::IsErrorCatastrophic.
  //
  // |catastrophic_error_handler| must be initialized (i.e. !is_null()).
  //
  // A single operation (like Load or SaveChanges) may result in the
  // |catastrophic_error_handler| being invoked several times.
  //
  // There can be at most one handler. If this method is invoked when there is
  // already a handler, the existing handler is overwritten with
  // |catastrophic_error_handler|.
  virtual void SetCatastrophicErrorHandler(
      const base::Closure& catastrophic_error_handler);

  // Returns true on success, false on error.
  bool GetDatabasePageSize(int* page_size);

  bool ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                         const std::string& dump_name);

 protected:
  // For test classes.
  DirectoryBackingStore(
      const std::string& dir_name,
      const base::RepeatingCallback<std::string()>& cache_guid_generator,
      sql::Database* connection);

  // An accessor for the underlying sql::Database. Avoid using outside of
  // tests.
  sql::Database* db();

  // Return true if the DB is open.
  bool IsOpen() const;

  // Open the DB at |path|.
  // Return true on success, false on failure.
  bool Open(const base::FilePath& path);

  // Open an in memory DB.
  // Return true on success, false on failure.
  bool OpenInMemory();

  // Initialize database tables. Return true on success, false on error.
  // |did_start_new| must not be null and allows callers to know whether a
  // previously existing directory was opened or a new empty one had to be
  // initialized.
  bool InitializeTables(bool* did_start_new);

  // Load helpers for entries and attributes. Return true on success, false on
  // error.
  bool LoadEntries(Directory::MetahandlesMap* handles_map,
                   MetahandleSet* metahandles_to_purge);
  bool LoadInfo(Directory::KernelLoadInfo* info);

  enum EntryTable {
    METAS_TABLE,
  };
  // Removes each entry whose metahandle is in |handles| from the table
  // specified by |from| table. Does synchronous I/O.  Returns false on error.
  bool DeleteEntries(EntryTable from, const MetahandleSet& handles);

  // Serialization helpers for ModelType.  These convert between
  // the ModelType enum and the values we persist in the database to identify
  // a model.  We persist a default instance of the specifics protobuf as the
  // ID, rather than the enum value.
  static ModelType ModelIdToModelTypeEnum(const void* data, int length);
  static std::string ModelTypeEnumToModelId(ModelType model_type);

  // Checks that the references between sync nodes is consistent.
  static bool VerifyReferenceIntegrity(
      const Directory::MetahandlesMap* handles_map);

  // Migration utilities.
  bool RefreshColumns();
  bool SetVersion(int version);
  int GetVersion();

  // Individual version migrations.
  bool MigrateVersion67To68();
  bool MigrateVersion68To69();
  bool MigrateVersion69To70();
  bool MigrateVersion70To71();
  bool MigrateVersion71To72();
  bool MigrateVersion72To73();
  bool MigrateVersion73To74();
  bool MigrateVersion74To75();
  bool MigrateVersion75To76();
  bool MigrateVersion76To77();
  bool MigrateVersion77To78();
  bool MigrateVersion78To79();
  bool MigrateVersion79To80();
  bool MigrateVersion80To81();
  bool MigrateVersion81To82();
  bool MigrateVersion82To83();
  bool MigrateVersion83To84();
  bool MigrateVersion84To85();
  bool MigrateVersion85To86();
  bool MigrateVersion86To87();
  bool MigrateVersion87To88();
  bool MigrateVersion88To89();
  bool MigrateVersion89To90();
  bool MigrateVersion90To91();
  bool MigrateVersion91To92();

  // Accessor for needs_column_refresh_.  Used in tests.
  bool needs_column_refresh() const;

  // Destroys the existing Connection and creates a new one.
  void ResetAndCreateConnection();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class DirectoryBackingStoreTest;
  friend class TestDirectoryBackingStore;
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest,
                           IncreaseDatabasePageSizeFrom4KTo32K);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest,
                           CatastrophicErrorHandler_KeptAcrossReset);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest,
                           CatastrophicErrorHandler_InvocationDuringLoad);
  FRIEND_TEST_ALL_PREFIXES(
      DirectoryBackingStoreTest,
      CatastrophicErrorHandler_InvocationDuringSaveChanges);
  FRIEND_TEST_ALL_PREFIXES(MigrationTest, ToCurrentVersion);

  // Drop all tables in preparation for reinitialization.
  void DropAllTables();

  bool SafeToPurgeOnLoading(const EntryKernel& entry) const;

  // Drops a table if it exists, harmless if the table did not already exist.
  bool SafeDropTable(const char* table_name);

  bool CreateTables();

  // Create 'share_info' or 'temp_share_info' depending on value of
  // is_temporary. Returns true on success, false on error.
  bool CreateShareInfoTable(bool is_temporary);
  bool CreateShareInfoTableVersion71(bool is_temporary);

  // Create 'metas' or 'temp_metas' depending on value of is_temporary. Also
  // create a 'deleted_metas' table using same schema. Returns true on success,
  // false on error.
  bool CreateMetasTable(bool is_temporary);

  // Returns true on success, false on error.
  bool CreateModelsTable();
  bool CreateV71ModelsTable();
  bool CreateV75ModelsTable();
  bool CreateV81ModelsTable();

  // Returns true on success, false on error.
  bool MigrateToSpecifics(
      const char* old_columns,
      const char* specifics_column,
      void (*handler_function)(sql::Statement* old_value_query,
                               int old_value_column,
                               sync_pb::EntitySpecifics* mutable_new_value));

  // Returns true on success, false on error.
  bool Vacuum();

  // Returns true on success, false on error.
  bool UpdatePageSizeIfNecessary();

  // Prepares |save_statement| for saving entries in |table|.
  void PrepareSaveEntryStatement(EntryTable table,
                                 sql::Statement* save_statement);

  const std::string dir_name_;
  const base::RepeatingCallback<std::string()> cache_guid_generator_;
  const int database_page_size_;

  std::unique_ptr<sql::Database> db_;
  sql::Statement save_meta_statement_;

  // Set to true if migration left some old columns around that need to be
  // discarded.
  bool needs_metas_column_refresh_;
  bool needs_share_info_column_refresh_;

  // We keep a copy of the Closure so we reinstall it when the underlying
  // sql::Database is destroyed/recreated.
  base::Closure catastrophic_error_handler_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryBackingStore);
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_
