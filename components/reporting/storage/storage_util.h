// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_UTIL_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_UTIL_H_

#include <string>
#include <tuple>
#include <unordered_set>

#include "base/files/file_path.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// StorageDirectory is a non-thread-safe static class that executes operations
// on a `Storage` directory that contains `StorageQueue` directories.
class StorageDirectory {
 public:
  // Metadata file name prefix.
  static constexpr base::FilePath::CharType kMetadataFileNamePrefix[] =
      FILE_PATH_LITERAL("META");

  struct Hash {
    size_t operator()(Priority priority) const noexcept {
      static constexpr std::hash<Priority> priority_hasher;
      return priority_hasher(priority);
    }
  };
  using Set = std::unordered_set<Priority, Hash>;

  // Returns a set of <Priority, GenerationGuid> tuples corresponding to valid
  // queue directories found in the storage directory provided in `options`. For
  // legacy directories, GenerationGuid will be empty.
  static Set FindQueueDirectories(
      const base::FilePath& storage_directory,
      const StorageOptions::QueuesOptionsList& options_list);

 private:
  // Return priority from a filepath, or error status.
  static StatusOr<Priority> ParsePriorityFromQueueDirectory(
      const base::FilePath& full_path,
      const StorageOptions::QueuesOptionsList& options_list);

  // Returns true if the filepath matches the format of a metadata file. Returns
  // false otherwise.
  static bool IsMetaDataFile(const base::FilePath& filepath);
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_UTIL_H_
