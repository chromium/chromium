// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/file_system_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_delegate.h"

namespace device_signals {

class FileSystemServiceImpl : public FileSystemService {
 public:
  explicit FileSystemServiceImpl(std::unique_ptr<PlatformDelegate> delegate);
  ~FileSystemServiceImpl() override;

  // FileSystemService:
  std::vector<FileSystemItem> GetSignals(
      const std::vector<GetFileSystemInfoOptions>& options) override;
  PresenceValue ResolveFileSystemItem(
      const base::FilePath& original_file_path,
      base::FilePath* resolved_file_path) const override;

 private:
  std::unique_ptr<PlatformDelegate> delegate_;
};

// static
std::unique_ptr<FileSystemService> FileSystemService::Create(
    std::unique_ptr<PlatformDelegate> delegate) {
  return std::make_unique<FileSystemServiceImpl>(std::move(delegate));
}

FileSystemServiceImpl::FileSystemServiceImpl(
    std::unique_ptr<PlatformDelegate> delegate)
    : delegate_(std::move(delegate)) {}

FileSystemServiceImpl::~FileSystemServiceImpl() = default;

std::vector<FileSystemItem> FileSystemServiceImpl::GetSignals(
    const std::vector<GetFileSystemInfoOptions>& options) {
  std::vector<FileSystemItem> collected_items;
  for (const auto& option : options) {
    FileSystemItem collected_item;
    collected_item.file_path = option.file_path;

    // The resolved file path will be used internally for signal collection, but
    // won't be returned to the caller.
    base::FilePath resolved_file_path;
    collected_item.presence =
        ResolveFileSystemItem(option.file_path, &resolved_file_path);

    collected_items.push_back(std::move(collected_item));
  }

  return collected_items;
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
