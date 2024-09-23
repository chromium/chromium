// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/configure_app_container_sandbox.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr wchar_t kBaseDirDacl[] = L"D:P(A;OICI;FA;;;WD)";
constexpr wchar_t kConfiguredDirDacl[] =
    L"D:PAI(A;OICI;0x1200a9;;;S-1-15-3-1024-3424233489-972189580-2057154623-"
    L"747635277-1604371224-316187997-3786583170-1043257646)(A;OICI;0x1200a9;;;"
    L"S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-"
    L"4240214049-4028510897-3317428798)(A;OICI;FA;;;WD)";

}  // namespace

TEST(ConfigureAppContainerSandboxTest, ConfigureAppContainerSandbox) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.GetPath().Append(L"testdir");
  ASSERT_TRUE(base::CreateWithDacl(path, kBaseDirDacl, true));
  EXPECT_EQ(kBaseDirDacl, base::GetFileDacl(path));

  ASSERT_TRUE(ConfigureAppContainerSandbox(
      std::array<const base::FilePath*, 1>{&path}));
  EXPECT_EQ(kConfiguredDirDacl, base::GetFileDacl(path));
}

TEST(ConfigureAppContainerSandboxTest,
     ConfigureAppContainerSandboxForMultipleDirs) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath pathA = temp_dir.GetPath().Append(L"dirA");
  ASSERT_TRUE(base::CreateWithDacl(pathA, kBaseDirDacl, true));

  base::FilePath pathB = temp_dir.GetPath().Append(L"dirB");
  ASSERT_TRUE(base::CreateWithDacl(pathB, kBaseDirDacl, true));

  ASSERT_TRUE(ConfigureAppContainerSandbox(
      std::array<const base::FilePath*, 2>{&pathA, &pathB}));
  EXPECT_EQ(kConfiguredDirDacl, base::GetFileDacl(pathA));
  EXPECT_EQ(kConfiguredDirDacl, base::GetFileDacl(pathB));
}

TEST(ConfigureAppContainerSandboxTest, ReportSuccessOnEmptyList) {
  ASSERT_TRUE(ConfigureAppContainerSandbox({}));
}

}  // namespace installer
