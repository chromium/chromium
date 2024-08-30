// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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
  explicit UserAnnotationsDatabase(const base::FilePath& storage_dir);

  UserAnnotationsDatabase(const UserAnnotationsDatabase&) = delete;
  UserAnnotationsDatabase& operator=(const UserAnnotationsDatabase&) = delete;
  ~UserAnnotationsDatabase();

  // Updates the `entries` to database and returns whether it succeeded.
  bool UpdateEntries(
      const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
          entries);

  // Returns all the annotations from database.
  std::vector<optimization_guide::proto::UserAnnotationsEntry>
  RetrieveAllEntries();

 private:
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_DATABASE_H_
