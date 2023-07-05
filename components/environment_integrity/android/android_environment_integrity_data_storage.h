// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_STORAGE_H_
#define COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_STORAGE_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "url/origin.h"

namespace environment_integrity {

class AndroidEnvironmentIntegrityDataStorage {
 public:
  explicit AndroidEnvironmentIntegrityDataStorage(
      const base::FilePath& path_to_database);

  AndroidEnvironmentIntegrityDataStorage(
      const AndroidEnvironmentIntegrityDataStorage&) = delete;
  AndroidEnvironmentIntegrityDataStorage& operator=(
      const AndroidEnvironmentIntegrityDataStorage&) = delete;
  AndroidEnvironmentIntegrityDataStorage(
      AndroidEnvironmentIntegrityDataStorage&&) = delete;
  AndroidEnvironmentIntegrityDataStorage& operator=(
      AndroidEnvironmentIntegrityDataStorage&&) = delete;

  ~AndroidEnvironmentIntegrityDataStorage();

  absl::optional<int64_t> GetHandle(const url::Origin& origin);

  void SetHandle(const url::Origin& origin, int64_t handle);

  void ClearData(
      content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher);

 private:
  bool EnsureDBInitialized() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeDB() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InitializeSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath path_to_database_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace environment_integrity

#endif  // COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_DATA_STORAGE_H_
