// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/file_system_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/executable_metadata_service.h"
#include "components/device_signals/core/system_signals/hashing_utils.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"

namespace device_signals {

namespace {

std::vector<FileSystemItem> GetAllItems(
    const FilePathMap<std::vector<FileSystemItem>>& map) {
  std::vector<FileSystemItem> items;
  for (const auto& pair : map) {
    items.insert(items.end(), pair.second.begin(), pair.second.end());
  }
  return items;
}

}  // namespace

class FileSystemServiceImpl : public FileSystemService {
 public:
  explicit FileSystemServiceImpl(
      std::unique_ptr<PlatformDelegate> delegate,
      std::unique_ptr<ExecutableMetadataService> executable_metadata_service);
  ~FileSystemServiceImpl() override;

  // FileSystemService:
  std::vector<FileSystemItem> GetSignals(
      const std::vector<GetFileSystemInfoOptions>& options) override;
  PresenceValue ResolveFileSystemItem(
      const base::FilePath& original_file_path,
      base::FilePath* resolved_file_path) const override;

 private:
  std::unique_ptr<PlatformDelegate> delegate_;
  std::unique_ptr<ExecutableMetadataService> executable_metadata_service_;
};

// static
std::unique_ptr<FileSystemService> FileSystemService::Create(
    std::unique_ptr<PlatformDelegate> delegate,
    std::unique_ptr<ExecutableMetadataService> executable_metadata_service) {
  return std::make_unique<FileSystemServiceImpl>(
      std::move(delegate), std::move(executable_metadata_service));
}

FileSystemServiceImpl::FileSystemServiceImpl(
    std::unique_ptr<PlatformDelegate> delegate,
    std::unique_ptr<ExecutableMetadataService> executable_metadata_service)
    : delegate_(std::move(delegate)),
      executable_metadata_service_(std::move(executable_metadata_service)) {
  DCHECK(delegate_);
  DCHECK(executable_metadata_service_);
}

FileSystemServiceImpl::~FileSystemServiceImpl() = default;

std::vector<FileSystemItem> FileSystemServiceImpl::GetSignals(
    const std::vector<GetFileSystemInfoOptions>& options) {
  // Keeping a map of resolved file paths to their corresponding
  // FileSystemItem objects is required in order to call the batch
  // GetAllExecutableMetadata API and having a way to map the resulting
  // ExecutableMetadata back to the corresponding FileSystemItem.
  // The value of the map is a vector since there is no guarantee that some of
  // the resolved file paths don't point to the same file.
  FilePathMap<std::vector<FileSystemItem>> resolved_paths_to_item_map;
  FilePathSet executable_paths;
  for (const auto& option : options) {
    FileSystemItem collected_item;
    collected_item.file_path = option.file_path;

    // The resolved file path will be used internally for signal collection, but
    // won't be returned to the caller.
    base::FilePath resolved_file_path;
    collected_item.presence =
        ResolveFileSystemItem(option.file_path, &resolved_file_path);

    // Only try to collect more signals if a file exists at the resolved path.
    if (collected_item.presence == PresenceValue::kFound &&
        !base::DirectoryExists(resolved_file_path)) {
      if (option.compute_sha256) {
        collected_item.sha256_hash = HashFile(resolved_file_path);
      }

      if (option.compute_executable_metadata) {
        if (!executable_paths.contains(resolved_file_path)) {
          executable_paths.insert(resolved_file_path);
        }
      }
    }

    if (!resolved_paths_to_item_map.contains(resolved_file_path)) {
      resolved_paths_to_item_map[resolved_file_path] =
          std::vector<FileSystemItem>();
    }

    resolved_paths_to_item_map[resolved_file_path].push_back(
        std::move(collected_item));
  }

  auto collected_executable_metadata =
      executable_metadata_service_->GetAllExecutableMetadata(executable_paths);

  for (const auto& path_metadata_pair : collected_executable_metadata) {
    if (!resolved_paths_to_item_map.contains(path_metadata_pair.first)) {
      continue;
    }

    for (auto& collected_item :
         resolved_paths_to_item_map[path_metadata_pair.first]) {
      collected_item.executable_metadata = path_metadata_pair.second;
    }
  }

  return GetAllItems(resolved_paths_to_item_map);
}

PresenceValue FileSystemServiceImpl::ResolveFileSystemItem(
    const base::FilePath& original_file_path,
    base::FilePath* resolved_file_path) const {
  base::FilePath local_resolved_path;
  if (delegate_->ResolveFilePath(original_file_path, &local_resolved_path)) {
    *resolved_file_path = local_resolved_path;
    if (delegate_->PathIsReadable(local_resolved_path)) {
      return PresenceValue::kFound;
    }
    return PresenceValue::kAccessDenied;
  }
  return PresenceValue::kNotFound;
}

}  // namespace device_signals
