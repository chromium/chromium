// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

#include <shlobj.h>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

TEST(PreFetchedPathsTest, PathsArePreFetched) {
  PreFetchedPaths* pre_fetched_paths = PreFetchedPaths::GetInstance();
  // Note: pre_fetched_paths->Initialized() is already called in test_main.cc.

  EXPECT_FALSE(pre_fetched_paths->GetExecutablePath().empty());
  EXPECT_FALSE(pre_fetched_paths->GetProgramFilesFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetWindowsFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCommonAppDataFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetLocalAppDataFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlProgramFilesFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlProgramFilesX86Folder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlWindowsFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlStartupFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlSystemFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlCommonAppDataFolder().empty());
  EXPECT_FALSE(pre_fetched_paths->GetCsidlLocalAppDataFolder().empty());
}

TEST(PreFetchedPathsTest, CheckExpectedPaths) {
  PreFetchedPaths* pre_fetched_paths = PreFetchedPaths::GetInstance();
  // Note: pre_fetched_paths->Initialized() is already called in test_main.cc.

  EXPECT_EQ(base::PathService::CheckedGet(base::FILE_EXE),
            pre_fetched_paths->GetExecutablePath());
  EXPECT_EQ(base::PathService::CheckedGet(base::DIR_PROGRAM_FILES),
            pre_fetched_paths->GetProgramFilesFolder());
  EXPECT_EQ(base::PathService::CheckedGet(base::DIR_WINDOWS),
            pre_fetched_paths->GetWindowsFolder());
  EXPECT_EQ(base::PathService::CheckedGet(base::DIR_COMMON_APP_DATA),
            pre_fetched_paths->GetCommonAppDataFolder());
  EXPECT_EQ(base::PathService::CheckedGet(base::DIR_LOCAL_APP_DATA),
            pre_fetched_paths->GetLocalAppDataFolder());
  EXPECT_EQ(
      base::PathService::CheckedGet(CsidlToPathServiceKey(CSIDL_PROGRAM_FILES)),
      pre_fetched_paths->GetCsidlProgramFilesFolder());
  EXPECT_EQ(base::PathService::CheckedGet(
                CsidlToPathServiceKey(CSIDL_PROGRAM_FILESX86)),
            pre_fetched_paths->GetCsidlProgramFilesX86Folder());
  EXPECT_EQ(base::PathService::CheckedGet(CsidlToPathServiceKey(CSIDL_WINDOWS)),
            pre_fetched_paths->GetCsidlWindowsFolder());
  EXPECT_EQ(base::PathService::CheckedGet(CsidlToPathServiceKey(CSIDL_STARTUP)),
            pre_fetched_paths->GetCsidlStartupFolder());
  EXPECT_EQ(base::PathService::CheckedGet(CsidlToPathServiceKey(CSIDL_SYSTEM)),
            pre_fetched_paths->GetCsidlSystemFolder());
  EXPECT_EQ(base::PathService::CheckedGet(
                CsidlToPathServiceKey(CSIDL_COMMON_APPDATA)),
            pre_fetched_paths->GetCsidlCommonAppDataFolder());
  EXPECT_EQ(
      base::PathService::CheckedGet(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA)),
      pre_fetched_paths->GetCsidlLocalAppDataFolder());
}

}  // namespace

}  // namespace chrome_cleaner
