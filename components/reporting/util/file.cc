// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/file.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace reporting {

bool DeleteFileWarnIfFailed(const base::FilePath& path) {
  const auto delete_result = base::DeleteFile(path);
  if (!delete_result) {
    LOG(WARNING) << "Failed to delete " << path.MaybeAsASCII();
  }
  return delete_result;
}

bool DeleteFilesWarnIfFailed(
    base::FileEnumerator& dir_enum,
    base::RepeatingCallback<bool(const base::FilePath&)> pred) {
  std::vector<base::FilePath> files_to_delete;
  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    if (pred.Run(full_name)) {
      files_to_delete.push_back(std::move(full_name));
    }
  }
  bool success = true;
  for (const auto& file_to_delete : files_to_delete) {
    if (!DeleteFileWarnIfFailed(file_to_delete)) {
      success = false;
    }
  }
  return success;
}
}  // namespace reporting
