// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/base_platform_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/device_signals/core/common/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

namespace {

constexpr base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("test_file");

}  // namespace

class TestPlatformDelegate : public BasePlatformDelegate {
 public:
  TestPlatformDelegate() = default;
  ~TestPlatformDelegate() override = default;

  // Uninterested in pure virtual functions.
  MOCK_METHOD(bool,
              ResolveFilePath,
              (const base::FilePath&, base::FilePath*),
              (override));
  MOCK_METHOD(std::optional<ProductMetadata>,
              GetProductMetadata,
              (const base::FilePath&),
              (override));
  MOCK_METHOD(std::optional<SigningCertificatesPublicKeys>,
              GetSigningCertificatesPublicKeys,
              (const base::FilePath&),
              (override));
};

class BasePlatformDelegateTest : public testing::Test {
 protected:
  BasePlatformDelegateTest() {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    file_path_ = scoped_dir_.GetPath().Append(kFileName);
    EXPECT_TRUE(base::WriteFile(file_path_, "irrelevant file content"));
  }

  base::ScopedTempDir scoped_dir_;
  base::FilePath file_path_;
  TestPlatformDelegate platform_delegate_;
};

// Sanity checks for functions that wrap base namespace functions. Only tests
// success cases as underlying functions are tested on their own.
TEST_F(BasePlatformDelegateTest, SanityChecks) {
  EXPECT_TRUE(platform_delegate_.PathIsReadable(file_path_));
  EXPECT_FALSE(platform_delegate_.DirectoryExists(file_path_));
  EXPECT_TRUE(platform_delegate_.DirectoryExists(scoped_dir_.GetPath()));
}

}  // namespace device_signals
