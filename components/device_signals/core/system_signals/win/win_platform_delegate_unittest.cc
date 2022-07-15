// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"

#include <array>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

namespace {

// Using regular strings instead of file path literals as they will be used
// to construct all sorts of file paths, and also non-file-paths.
constexpr char kEnvironmentVariableName[] = "TestEnvironmentVariablePath";
constexpr char kTestFileName[] = "test_file";

constexpr base::FilePath::CharType kInexistantFileName[] =
    FILE_PATH_LITERAL("does_not_exit");

}  // namespace

class WinPlatformDelegateTest : public testing::Test {
 protected:
  WinPlatformDelegateTest() : env_(base::Environment::Create()) {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    absolute_file_path_ = scoped_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe(kTestFileName));
    EXPECT_TRUE(
        base::WriteFile(absolute_file_path_, "irrelevant file content"));

    env_->SetVar(kEnvironmentVariableName,
                 scoped_dir_.GetPath().AsUTF8Unsafe());
  }

  ~WinPlatformDelegateTest() override {
    env_->UnSetVar(kEnvironmentVariableName);
  }

  base::ScopedTempDir scoped_dir_;
  base::FilePath absolute_file_path_;
  std::unique_ptr<base::Environment> env_;
  WinPlatformDelegate platform_delegate_;
};

TEST_F(WinPlatformDelegateTest, ResolveFilePath_Success) {
  std::string directory_name = scoped_dir_.GetPath().BaseName().AsUTF8Unsafe();

  std::array<std::string, 4> test_cases = {
      absolute_file_path_.AsUTF8Unsafe(),
      base::StrCat({"%", kEnvironmentVariableName, "%\\", kTestFileName}),
      base::StrCat({"%", kEnvironmentVariableName, "%\\..\\", directory_name,
                    "\\", kTestFileName}),

      // Should work with directories too.
      scoped_dir_.GetPath().AsUTF8Unsafe()};

  for (const auto& test_case : test_cases) {
    base::FilePath resolved_fp;
    EXPECT_TRUE(platform_delegate_.ResolveFilePath(
        base::FilePath::FromUTF8Unsafe(test_case), &resolved_fp));
  }
}

TEST_F(WinPlatformDelegateTest, ResolveFilePath_Fail) {
  base::FilePath resolved_fp;
  EXPECT_FALSE(platform_delegate_.ResolveFilePath(
      scoped_dir_.GetPath().Append(kInexistantFileName), &resolved_fp));
  EXPECT_EQ(resolved_fp, base::FilePath());
}

}  // namespace device_signals
