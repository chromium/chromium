// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_executable_metadata_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

WinExecutableMetadataService::WinExecutableMetadataService(
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : ExecutableMetadataService(std::move(platform_delegate)) {}

WinExecutableMetadataService::~WinExecutableMetadataService() = default;

FilePathMap<ExecutableMetadata>
WinExecutableMetadataService::GetAllExecutableMetadata(
    const FilePathSet& file_paths) {
  FilePathMap<bool> files_are_running_map =
      platform_delegate_->AreExecutablesRunning(file_paths);

  FilePathMap<ExecutableMetadata> file_paths_to_metadata_map;
  for (const auto& file_path : file_paths) {
    ExecutableMetadata executable_metadata;

    if (files_are_running_map.contains(file_path)) {
      executable_metadata.is_running = files_are_running_map[file_path];
    }

    // TODO(b:231472950): Add public key hash signal.
    // TODO(b:231472965): Add product version and name signals.
    file_paths_to_metadata_map[file_path] = executable_metadata;
  }

  return file_paths_to_metadata_map;
}

}  // namespace device_signals
