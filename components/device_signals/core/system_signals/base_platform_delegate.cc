// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/base_platform_delegate.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_iterator.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/platform_utils.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/file_version_info.h"
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

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

    std::optional<base::FilePath> exe_path =
        GetProcessExePath(process_entry->pid());
    if (exe_path && running_map.contains(exe_path.value())) {
      ++counter;
      running_map[exe_path.value()] = true;
    }
  }

  return running_map;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

std::optional<PlatformDelegate::ProductMetadata>
BasePlatformDelegate::GetProductMetadata(const base::FilePath& file_path) {
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(file_path));

  if (!version_info) {
    return std::nullopt;
  }

  std::u16string product_name;
  if (!version_info->product_name().empty()) {
    product_name = version_info->product_name();
  } else if (!version_info->product_short_name().empty()) {
    product_name = version_info->product_short_name();
  }

  std::u16string version;
  if (!version_info->product_version().empty()) {
    version = version_info->product_version();
  } else if (!version_info->file_version().empty()) {
    version = version_info->file_version();
  }

  PlatformDelegate::ProductMetadata product_metadata;
  if (!product_name.empty()) {
    product_metadata.name = base::UTF16ToUTF8(product_name);
  }

  if (!version.empty()) {
    product_metadata.version = base::UTF16ToUTF8(version);
  }

  return product_metadata;
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace device_signals
