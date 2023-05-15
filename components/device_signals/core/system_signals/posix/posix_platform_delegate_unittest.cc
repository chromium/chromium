// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/posix/posix_platform_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

namespace {

constexpr char kHome2EnvVariableName[] = "Home2";
constexpr char kNonsenseEnvVariableName[] = "7D849956400F45A9985648574F4C72E4";

constexpr base::FilePath::CharType kTestFileName[] =
    FILE_PATH_LITERAL("test_filename");
}  // namespace

class PosixPlatformDelegateTest : public testing::Test {
 protected:
  PosixPlatformDelegateTest() {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    binary_path_ = scoped_dir_.GetPath().Append(base::FilePath(kTestFileName));
    home_dir_path_ = base::GetHomeDir().Append(base::FilePath(kTestFileName));
    EXPECT_TRUE(base::WriteFile(
        binary_path_, base::Uuid::GenerateRandomV4().AsLowercaseString()));
    EXPECT_TRUE(base::WriteFile(
        home_dir_path_, base::Uuid::GenerateRandomV4().AsLowercaseString()));
  }

  base::ScopedTempDir scoped_dir_;
  base::FilePath home_dir_path_;
  base::FilePath binary_path_;
  PosixPlatformDelegate platform_delegate_;
};

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_Absolute) {
  base::FilePath resolved_file_path;
  EXPECT_TRUE(
      platform_delegate_.ResolveFilePath(binary_path_, &resolved_file_path));
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, binary_path_));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_Tilde) {
  base::FilePath resolved_file_path;
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(base::StringPrintf("~/%s", kTestFileName)),
      &resolved_file_path));
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, home_dir_path_));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_EnvVar) {
  base::ScopedEnvironmentVariableOverride env_override(kHome2EnvVariableName,
                                                       home_dir_path_.value());

  base::FilePath resolved_file_path;
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s", kHome2EnvVariableName)),
      &resolved_file_path));
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, home_dir_path_));

  // Test with a separator suffix too.
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s/", kHome2EnvVariableName)),
      &resolved_file_path));
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, home_dir_path_));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_EnvVarToTilde) {
  base::ScopedEnvironmentVariableOverride env_override(kHome2EnvVariableName,
                                                       "~");

  base::FilePath resolved_file_path;
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s/%s", kHome2EnvVariableName, kTestFileName)),
      &resolved_file_path));
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, home_dir_path_));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_InvalidEnvVar) {
  base::FilePath resolved_file_path;
  EXPECT_FALSE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s", kNonsenseEnvVariableName)),
      &resolved_file_path));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_CyclicEnvVar) {
  base::ScopedEnvironmentVariableOverride env_override(
      kHome2EnvVariableName, base::StringPrintf("$%s", kHome2EnvVariableName));

  base::FilePath resolved_file_path;
  EXPECT_FALSE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s", kHome2EnvVariableName)),
      &resolved_file_path));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_EnvVarMissingSeparator) {
  // Tests the "$Home2test_filename" case, which is treated as invalid.
  base::ScopedEnvironmentVariableOverride env_override(
      kHome2EnvVariableName, scoped_dir_.GetPath().value());

  base::FilePath resolved_file_path;
  EXPECT_FALSE(platform_delegate_.ResolveFilePath(
      base::FilePath::FromUTF8Unsafe(
          base::StringPrintf("$%s%s", kHome2EnvVariableName, kTestFileName)),
      &resolved_file_path));
}

TEST_F(PosixPlatformDelegateTest, ResolveFilePath_PathTraversal) {
  auto file_base_name = binary_path_.BaseName();
  auto file_path_with_traversal = binary_path_.DirName();
  auto bundle_dir_base_name = file_path_with_traversal.BaseName();
  file_path_with_traversal = file_path_with_traversal.Append("..")
                                 .Append(bundle_dir_base_name)
                                 .Append(file_base_name);

  base::FilePath resolved_file_path;
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(file_path_with_traversal,
                                                 &resolved_file_path));

  // Final resolved path should point to the binary directly.
  EXPECT_TRUE(base::ContentsEqual(resolved_file_path, binary_path_));
}

}  // namespace device_signals
