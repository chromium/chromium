// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/executable_metadata_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/mock_platform_delegate.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace device_signals {

namespace {

ExecutableMetadata CreateExecutableMetadata(
    bool is_running,
    const std::optional<std::vector<std::string>>& public_keys_hashes) {
  ExecutableMetadata metadata;
  metadata.is_running = is_running;
  metadata.public_keys_hashes = public_keys_hashes;
  return metadata;
}

}  // namespace

class ExecutableMetadataServiceTest : public testing::Test {
 protected:
  ExecutableMetadataServiceTest() {
    auto mock_platform_delegate = std::make_unique<MockPlatformDelegate>();
    mock_platform_delegate_ = mock_platform_delegate.get();

    executable_metadata_service_ =
        ExecutableMetadataService::Create(std::move(mock_platform_delegate));
  }

  std::unique_ptr<ExecutableMetadataService> executable_metadata_service_;
  raw_ptr<MockPlatformDelegate> mock_platform_delegate_;
};

TEST_F(ExecutableMetadataServiceTest, GetAllExecutableMetadata_Empty) {
  FilePathSet empty_set;

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(empty_set))
      .WillOnce(Return(FilePathMap<bool>()));

  FilePathMap<ExecutableMetadata> empty_map;
  EXPECT_EQ(executable_metadata_service_->GetAllExecutableMetadata(empty_set),
            empty_map);
}

TEST_F(ExecutableMetadataServiceTest, GetAllExecutableMetadata_Success) {
  base::FilePath running_path =
      base::FilePath::FromUTF8Unsafe("C:\\some\\running\\file\\path.exe");
  base::FilePath not_running_path =
      base::FilePath::FromUTF8Unsafe("C:\\some\\file\\path.exe");

  FilePathSet executable_files;
  executable_files.insert(running_path);
  executable_files.insert(not_running_path);

  FilePathMap<bool> is_running_map;
  is_running_map.insert({running_path, true});
  is_running_map.insert({not_running_path, false});

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(executable_files))
      .WillOnce(Return(is_running_map));

  PlatformDelegate::SigningCertificatesPublicKeys expected_keys;
  expected_keys.hashes = std::vector<std::string>{"fake_public_key_value"};
  EXPECT_CALL(*mock_platform_delegate_,
              GetSigningCertificatesPublicKeys(running_path))
      .WillOnce(Return(expected_keys));
  EXPECT_CALL(*mock_platform_delegate_,
              GetSigningCertificatesPublicKeys(not_running_path))
      .WillOnce(Return(std::nullopt));

  FilePathMap<ExecutableMetadata> expected_metadata_map;
  expected_metadata_map.insert(
      {running_path, CreateExecutableMetadata(true, expected_keys.hashes)});
  expected_metadata_map.insert(
      {not_running_path, CreateExecutableMetadata(false, std::nullopt)});

  EXPECT_EQ(
      executable_metadata_service_->GetAllExecutableMetadata(executable_files),
      expected_metadata_map);
}

}  // namespace device_signals
