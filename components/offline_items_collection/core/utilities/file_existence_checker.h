// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UTILITIES_FILE_EXISTENCE_CHECKER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UTILITIES_FILE_EXISTENCE_CHECKER_H_

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"

namespace offline_items_collection {

// Class that checks in bulk, which of the provided set of file paths are not
// pointing to an existing file.
class FileExistenceChecker {
 public:
  // Collection of file paths and corresponding IDs, to be used by this class.
  template <typename T>
  using FileWithIdCollection = std::vector<std::pair<base::FilePath, T>>;

  // Callback through which the result (e.g. a collection of items detected to
  // be missing) is passed back.
  template <typename T>
  using ResultCallback = base::OnceCallback<void(FileWithIdCollection<T>)>;

  FileExistenceChecker(const FileExistenceChecker&) = delete;
  FileExistenceChecker& operator=(const FileExistenceChecker&) = delete;

  // Checks which of the provided |items_to_check| don't point to existing
  // files.
  template <typename T>
  static void CheckForMissingFiles(
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      FileWithIdCollection<T> items_to_check,
      ResultCallback<T> callback) {
    blocking_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileExistenceChecker::CheckForMissingFilesBlocking<T>,
                       std::move(items_to_check)),
        std::move(callback));
  }

 private:
  template <typename T>
  static FileWithIdCollection<T> CheckForMissingFilesBlocking(
      FileWithIdCollection<T> items_to_check) {
    FileWithIdCollection<T> missing_items;
    for (auto item : items_to_check) {
      if (!base::PathExists(item.first))
        missing_items.push_back(item);
    }
    return missing_items;
  }
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UTILITIES_FILE_EXISTENCE_CHECKER_H_
