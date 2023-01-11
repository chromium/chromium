// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/task/sql_store_base.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Database;
}

namespace offline_pages {
typedef StoreUpdateResult<OfflinePageItem> OfflinePagesUpdateResult;

// OfflinePageMetadataStore keeps metadata for the offline pages in an SQLite
// database.
//
// When updating the schema, be sure to do the following:
// * Increment the version number kCurrentVersion (let's call its new value N).
// * Write a function "UpgradeFromVersion<N-1>ToVersion<N>". This function
//   should upgrade an existing database of the (previously) latest version and
//   should call meta_table->SetVersionNumber(N). Add a case for version N-1 to
//   the loop in CreateSchema.
// * Update CreateLatestSchema() as necessary: this function creates a new empty
//   DB. If there were changes to existing tables, their original "CREATE"
//   queries should be copied into the new "UpgradeFromVersion..." function.
// * Update `kCompatibleVersion` when a new schema becomes incompatible with
//   old code (for instance, if a column is removed). Change it to the earliest
//   version that is compatible with the new schema; that is very likely to be
//   the version that broke compatibility.
// * Add a test for upgrading to the latest database version to
//   offline_page_metadata_store_unittest.cc. Good luck.
//
class OfflinePageMetadataStore : public SqlStoreBase {
 public:
  // This is the first version saved in the meta table, which was introduced in
  // the store in M65. It is set once a legacy upgrade is run successfully for
  // the last time in |UpgradeFromLegacyVersion|.
  static const int kFirstPostLegacyVersion = 1;
  static const int kCurrentVersion = 4;
  static const int kCompatibleVersion = kFirstPostLegacyVersion;

  // TODO(fgorski): Move to private and expose ForTest factory.
  // Applies in PrefetchStore as well.
  // Creates the store in memory. Should only be used for testing.
  explicit OfflinePageMetadataStore(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Creates the store with database pointing to provided directory.
  OfflinePageMetadataStore(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);

  ~OfflinePageMetadataStore() override;

  // Helper function used to force incorrect state for testing purposes.
  StoreState GetStateForTesting() const;

 protected:
  // SqlStoreBase:
  base::OnceCallback<bool(sql::Database* db)> GetSchemaInitializationFunction()
      override;
  void OnOpenStart(base::TimeTicks last_open_time) override;
  void OnOpenDone(bool success) override;
  void OnTaskBegin(bool is_initialized) override;
  void OnTaskRunComplete() override;
  void OnTaskReturnComplete() override;
  void OnCloseStart(InitializationStatus status_before_close) override;
  void OnCloseComplete() override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_
