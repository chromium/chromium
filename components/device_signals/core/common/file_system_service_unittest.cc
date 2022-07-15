// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/file_system_service.h"

#include <array>
#include <vector>

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/mock_platform_delegate.h"
#include "components/device_signals/core/common/platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace device_signals {

namespace {

GetFileSystemInfoOptions CreateOptions(const base::FilePath& path,
                                       bool compute_sha256,
                                       bool compute_is_executable) {
  GetFileSystemInfoOptions options;
  options.file_path = path;
  options.compute_sha256 = compute_sha256;
  options.compute_is_executable = compute_is_executable;
  return options;
}

}  // namespace

class FileSystemServiceTest : public testing::Test {
 protected:
  FileSystemServiceTest() {
    auto mock_platform_delegate =
        std::make_unique<testing::StrictMock<MockPlatformDelegate>>();
    mock_platform_delegate_ = mock_platform_delegate.get();

    file_system_service_ =
        FileSystemService::Create(std::move(mock_platform_delegate));
  }

  void ExpectResolvablePath(const base::FilePath& path,
                            const base::FilePath& resolved_path) {
    EXPECT_CALL(*mock_platform_delegate_, ResolveFilePath(path, _))
        .WillOnce(
            Invoke([&resolved_path](const base::FilePath& original_file_path,
                                    base::FilePath* resolved_file_path) {
              *resolved_file_path = resolved_path;
              return true;
            }));
  }

  void ExpectPathIsReadable(const base::FilePath& path) {
    EXPECT_CALL(*mock_platform_delegate_, PathIsReadable(path))
        .WillOnce(Return(true));
  }

  testing::StrictMock<MockPlatformDelegate>* mock_platform_delegate_;
  std::unique_ptr<FileSystemService> file_system_service_;
};

// Tests all possible PresenceValue outcomes.
TEST_F(FileSystemServiceTest, GetSignals_Presence) {
  base::FilePath unresolvable_file_path =
      base::FilePath::FromUTF8Unsafe("/cannot/resolve");
  EXPECT_CALL(*mock_platform_delegate_,
              ResolveFilePath(unresolvable_file_path, _))
      .WillOnce(Return(false));

  base::FilePath access_denied_path =
      base::FilePath::FromUTF8Unsafe("/cannot/access");
  base::FilePath access_denied_path_resolved =
      base::FilePath::FromUTF8Unsafe("/cannot/access/resolved");
  ExpectResolvablePath(access_denied_path, access_denied_path_resolved);
  EXPECT_CALL(*mock_platform_delegate_,
              PathIsReadable(access_denied_path_resolved))
      .WillOnce(Return(false));

  base::FilePath found_path = base::FilePath::FromUTF8Unsafe("/found");
  base::FilePath found_path_resolved =
      base::FilePath::FromUTF8Unsafe("/found/resolved");
  ExpectResolvablePath(found_path, found_path_resolved);
  ExpectPathIsReadable(found_path_resolved);

  std::vector<GetFileSystemInfoOptions> options;
  options.push_back(CreateOptions(unresolvable_file_path, false, false));
  options.push_back(CreateOptions(access_denied_path, false, false));
  options.push_back(CreateOptions(found_path, false, false));

  std::array<PresenceValue, 4> expected_presence_values{
      PresenceValue::kNotFound, PresenceValue::kAccessDenied,
      PresenceValue::kFound};

  auto file_system_items = file_system_service_->GetSignals(options);

  ASSERT_EQ(file_system_items.size(), options.size());

  for (size_t i = 0; i < file_system_items.size(); i++) {
    EXPECT_EQ(file_system_items[i].file_path, options[i].file_path);
    EXPECT_EQ(file_system_items[i].presence, expected_presence_values[i]);
  }
}

}  // namespace device_signals
