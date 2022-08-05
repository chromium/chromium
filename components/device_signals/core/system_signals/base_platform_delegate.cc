// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/base_platform_delegate.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_iterator.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/platform_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

BasePlatformDelegate::BasePlatformDelegate() = default;
BasePlatformDelegate::~BasePlatformDelegate() = default;

bool BasePlatformDelegate::PathIsReadable(
    const base::FilePath& file_path) const {
  return base::PathIsReadable(file_path);
}

bool BasePlatformDelegate::DirectoryExists(
    const base::FilePath& file_path) const {
  return base::DirectoryExists(file_path);
}

FilePathMap<bool> BasePlatformDelegate::AreExecutablesRunning(
    const FilePathSet& file_paths) {
  // Initialize map with the given file paths.
  FilePathMap<bool> running_map;
  running_map.reserve(file_paths.size());
  for (const auto& file_path : file_paths) {
    // Default initialize as not running.
    running_map[file_path] = false;
  }

  // Use counter to keep track of how many entries were found, which can allow
  // for an earlier return if all executables were to be found early.
  size_t counter = 0;
  base::ProcessIterator process_iterator(nullptr);
  while (const auto* process_entry = process_iterator.NextProcessEntry()) {
    if (counter >= running_map.size()) {
      // Found all items we were looking for, so return early.
      break;
    }

    absl::optional<base::FilePath> exe_path =
        GetProcessExePath(process_entry->pid());
    if (exe_path && running_map.contains(exe_path.value())) {
      ++counter;
      running_map[exe_path.value()] = true;
    }
  }

  return running_map;
}

FilePathMap<ExecutableMetadata> BasePlatformDelegate::GetAllExecutableMetadata(
    const FilePathSet& file_paths) {
  FilePathMap<bool> files_are_running_map = AreExecutablesRunning(file_paths);

  FilePathMap<ExecutableMetadata> file_paths_to_metadata_map;
  for (const auto& file_path : file_paths) {
    ExecutableMetadata executable_metadata;

    if (files_are_running_map.contains(file_path)) {
      executable_metadata.is_running = files_are_running_map[file_path];
    }

    file_paths_to_metadata_map[file_path] = executable_metadata;
  }

  return file_paths_to_metadata_map;
}

}  // namespace device_signals
