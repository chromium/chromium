// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"
#include "sql/database.h"
#include "sql/init_status.h"

namespace user_annotations {

// Wraps the SQLite database that provides on-disk storage for user annotation
// entries. This class is expected to be created and accessed on a backend
// sequence.
class UserAnnotationsDatabase {
 public:
  // `storage_dir` will generally be the Profile directory where the DB will be
  // opened from, or created if not exists.
  UserAnnotationsDatabase(const base::FilePath& storage_dir,
                          os_crypt_async::Encryptor encryptor);

  UserAnnotationsDatabase(const UserAnnotationsDatabase&) = delete;
  UserAnnotationsDatabase& operator=(const UserAnnotationsDatabase&) = delete;
  ~UserAnnotationsDatabase();

  // Updates the `entries` to database and returns whether it succeeded.
  UserAnnotationsExecutionResult UpdateEntries(
      const UserAnnotationsEntries& entries);

  // Returns all the annotations from database.
  UserAnnotationsEntryRetrievalResult RetrieveAllEntries();

  // Remove the user annotation entry with `entry_id` and returns whether the
  // operation completed successfully. Returns true even when no entry is found.
  bool RemoveEntry(EntryID entry_id);

  // Removes all the user annotation entries and returns whether the
  // operation completed successfully. Returns true even when there are no
  // entries to delete.
  bool RemoveAllEntries();

  // Removes the user annotation entries that were last modified from
  // `delete_begin` to `delete_end`.
  void RemoveAnnotationsInRange(const base::Time& delete_begin,
                                const base::Time& delete_end);

 private:
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_) =
      sql::Database(sql::DatabaseOptions{});
  os_crypt_async::Encryptor encryptor_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_
