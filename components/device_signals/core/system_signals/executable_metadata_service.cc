// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/executable_metadata_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

ExecutableMetadataService::ExecutableMetadataService(
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : platform_delegate_(std::move(platform_delegate)) {
  DCHECK(platform_delegate_);
}

ExecutableMetadataService::~ExecutableMetadataService() = default;

class ExecutableMetadataServiceImpl : public ExecutableMetadataService {
 public:
  explicit ExecutableMetadataServiceImpl(
      std::unique_ptr<PlatformDelegate> platform_delegate);
  ~ExecutableMetadataServiceImpl() override;

  // ExecutableMetadataService:
  FilePathMap<ExecutableMetadata> GetAllExecutableMetadata(
      const FilePathSet& file_paths) override;
};

// static
std::unique_ptr<ExecutableMetadataService> ExecutableMetadataService::Create(
    std::unique_ptr<PlatformDelegate> platform_delegate) {
  return std::make_unique<ExecutableMetadataServiceImpl>(
      std::move(platform_delegate));
}

ExecutableMetadataServiceImpl::ExecutableMetadataServiceImpl(
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : ExecutableMetadataService(std::move(platform_delegate)) {
  DCHECK(platform_delegate_);
}

ExecutableMetadataServiceImpl::~ExecutableMetadataServiceImpl() = default;

FilePathMap<ExecutableMetadata>
ExecutableMetadataServiceImpl::GetAllExecutableMetadata(
    const FilePathSet& file_paths) {
  FilePathMap<bool> files_are_running_map =
      platform_delegate_->AreExecutablesRunning(file_paths);

  FilePathMap<ExecutableMetadata> file_paths_to_metadata_map;
  for (const auto& file_path : file_paths) {
    ExecutableMetadata executable_metadata;

    if (files_are_running_map.contains(file_path)) {
      executable_metadata.is_running = files_are_running_map[file_path];
    }

    auto product_metadata = platform_delegate_->GetProductMetadata(file_path);
    if (product_metadata) {
      executable_metadata.product_name = product_metadata->name;
      executable_metadata.version = product_metadata->version;
    }

    auto public_keys =
        platform_delegate_->GetSigningCertificatesPublicKeys(file_path);
    if (public_keys) {
      executable_metadata.public_keys_hashes = public_keys->hashes;
      executable_metadata.is_os_verified = public_keys->is_os_verified;
      executable_metadata.subject_name = public_keys->subject_name;
    }

    file_paths_to_metadata_map[file_path] = executable_metadata;
  }

  return file_paths_to_metadata_map;
}

}  // namespace device_signals
