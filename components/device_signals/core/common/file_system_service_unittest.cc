// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/file_system_service.h"

#include <array>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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

std::string HexEncodeHash(const std::string& hashed_data) {
  return base::ToLowerASCII(
      base::HexEncode(std::data(hashed_data), hashed_data.size()));
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
  options.push_back(CreateOptions(unresolvable_file_path, true, true));
  options.push_back(CreateOptions(access_denied_path, true, true));
  options.push_back(CreateOptions(found_path, false, false));

  std::array<PresenceValue, 4> expected_presence_values{
      PresenceValue::kNotFound, PresenceValue::kAccessDenied,
      PresenceValue::kFound};

  auto file_system_items = file_system_service_->GetSignals(options);

  ASSERT_EQ(file_system_items.size(), options.size());

  for (size_t i = 0; i < file_system_items.size(); i++) {
    EXPECT_EQ(file_system_items[i].file_path, options[i].file_path);
    EXPECT_EQ(file_system_items[i].presence, expected_presence_values[i]);

    // No metadata was collected, as it was only requested for the two files
    // that cannot be found/accessed.
    EXPECT_FALSE(file_system_items[i].sha256_hash.has_value());
    EXPECT_FALSE(file_system_items[i].executable_metadata.has_value());
  }
}

// Tests calculating the hash for a file when requested, but not for a
// directory.
TEST_F(FileSystemServiceTest, GetSignals_Hash_Success) {
  base::ScopedTempDir scoped_dir;
  ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());

  constexpr base::FilePath::CharType existing_file[] =
      FILE_PATH_LITERAL("existing_file");

  const base::FilePath absolute_file_path =
      scoped_dir.GetPath().Append(existing_file);

  // Write some content.
  ASSERT_TRUE(base::WriteFile(absolute_file_path, "some file content"));

  ExpectResolvablePath(absolute_file_path, absolute_file_path);
  ExpectPathIsReadable(absolute_file_path);
  ExpectResolvablePath(scoped_dir.GetPath(), scoped_dir.GetPath());
  ExpectPathIsReadable(scoped_dir.GetPath());

  std::vector<GetFileSystemInfoOptions> options;
  options.push_back(
      CreateOptions(absolute_file_path, /*compute_sha256=*/true, false));
  options.push_back(
      CreateOptions(scoped_dir.GetPath(), /*compute_sha256=*/true, false));

  auto file_system_items = file_system_service_->GetSignals(options);

  constexpr char expected_sha256_hash[] =
      "b05ffa4eea8fb5609d576a68c1066be3f99e4dc53d365a0ac2a78259b2dd91f9";
  ASSERT_EQ(file_system_items.size(), options.size());

  // The file's hash was computed.
  EXPECT_EQ(file_system_items[0].file_path, absolute_file_path);
  EXPECT_EQ(file_system_items[0].presence, PresenceValue::kFound);
  ASSERT_TRUE(file_system_items[0].sha256_hash.has_value());
  EXPECT_EQ(HexEncodeHash(file_system_items[0].sha256_hash.value()),
            expected_sha256_hash);

  // The directory does not have a hash.
  EXPECT_EQ(file_system_items[1].file_path, scoped_dir.GetPath());
  EXPECT_EQ(file_system_items[1].presence, PresenceValue::kFound);
  EXPECT_FALSE(file_system_items[1].sha256_hash.has_value());
}

}  // namespace device_signals
