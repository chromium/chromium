// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/linux/linux_executable_metadata_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/mock_platform_delegate.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace device_signals {

namespace {

ExecutableMetadata CreateExecutableMetadata(bool is_running) {
  ExecutableMetadata metadata;
  metadata.is_running = is_running;
  return metadata;
}

}  // namespace

class LinuxExecutableMetadataServiceTest : public testing::Test {
 protected:
  LinuxExecutableMetadataServiceTest() {
    auto mock_platform_delegate = std::make_unique<MockPlatformDelegate>();
    mock_platform_delegate_ = mock_platform_delegate.get();

    executable_metadata_service_ =
        std::make_unique<LinuxExecutableMetadataService>(
            std::move(mock_platform_delegate));
  }

  MockPlatformDelegate* mock_platform_delegate_;
  std::unique_ptr<LinuxExecutableMetadataService> executable_metadata_service_;
};

TEST_F(LinuxExecutableMetadataServiceTest, GetAllExecutableMetadata_Empty) {
  FilePathSet empty_set;

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(empty_set))
      .WillOnce(Return(FilePathMap<bool>()));

  FilePathMap<ExecutableMetadata> empty_map;
  EXPECT_EQ(executable_metadata_service_->GetAllExecutableMetadata(empty_set),
            empty_map);
}

TEST_F(LinuxExecutableMetadataServiceTest, GetAllExecutableMetadata_Success) {
  base::FilePath running_path =
      base::FilePath::FromUTF8Unsafe("/some/running/file/path");
  base::FilePath not_running_path =
      base::FilePath::FromUTF8Unsafe("/some/file/path");

  FilePathSet executable_files;
  executable_files.insert(running_path);
  executable_files.insert(not_running_path);

  FilePathMap<bool> is_running_map;
  is_running_map.insert({running_path, true});
  is_running_map.insert({not_running_path, false});

  EXPECT_CALL(*mock_platform_delegate_, AreExecutablesRunning(executable_files))
      .WillOnce(Return(is_running_map));

  FilePathMap<ExecutableMetadata> expected_metadata_map;
  expected_metadata_map.insert({running_path, CreateExecutableMetadata(true)});
  expected_metadata_map.insert(
      {not_running_path, CreateExecutableMetadata(false)});

  EXPECT_EQ(
      executable_metadata_service_->GetAllExecutableMetadata(executable_files),
      expected_metadata_map);
}

}  // namespace device_signals
