// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_util.h"

#include <shlobj.h>
#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

// Converts an unsigned long integral to an HRESULT to avoid the
// warning about a narrowing conversion.
HRESULT MakeHRESULT(unsigned long x) {
  return static_cast<HRESULT>(x);
}

}  // namespace

TEST(WinUtil, HRESULTFromUpdaterError) {
  EXPECT_EQ(HRESULTFromUpdaterError(0), MakeHRESULT(0xa0430000L));
  EXPECT_EQ(HRESULTFromUpdaterError(ERROR_ACCESS_DENIED),
            MakeHRESULT(0xa0430005));
  EXPECT_EQ(HRESULTFromUpdaterError(-1), -1);
  EXPECT_EQ(HRESULTFromUpdaterError(-10), -10);
}

TEST(WinUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

TEST(WinUtil, GetServiceDisplayName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            L" ", kUpdaterVersionUtf16}),
              GetServiceDisplayName(is_internal_service));
  }
}

TEST(WinUtil, GetServiceName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            kUpdaterVersionUtf16}),
              GetServiceName(is_internal_service));
  }
}

TEST(WinUtil, BuildMsiCommandLine) {
  EXPECT_STREQ(L"", BuildMsiCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                        base::FilePath(L"NotMsi.exe"))
                        .c_str());
  EXPECT_STREQ(
      L"msiexec arg1 arg2 arg3 REBOOT=ReallySuppress /qn /i \"c:\\my "
      L"path\\YesMsi.msi\" /log \"c:\\my path\\YesMsi.msi.log\"",
      BuildMsiCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                          base::FilePath(L"c:\\my path\\YesMsi.msi"))
          .c_str());
  EXPECT_STREQ(
      L"msiexec arg1 arg2 arg3 INSTALLERDATA=\"c:\\my path\\installer data "
      L"file.dat\" REBOOT=ReallySuppress /qn /i \"c:\\my "
      L"path\\YesMsi.msi\" /log \"c:\\my path\\YesMsi.msi.log\"",
      BuildMsiCommandLine(
          std::wstring(L"arg1 arg2 arg3"),
          base::FilePath(L"c:\\my path\\installer data file.dat"),
          base::FilePath(L"c:\\my path\\YesMsi.msi"))
          .c_str());
}

TEST(WinUtil, BuildExeCommandLine) {
  EXPECT_STREQ(L"", BuildExeCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                        base::FilePath(L"NotExe.msi"))
                        .c_str());
  EXPECT_STREQ(L"\"c:\\my path\\YesExe.exe\" arg1 arg2 arg3",
               BuildExeCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                   base::FilePath(L"c:\\my path\\YesExe.exe"))
                   .c_str());
  EXPECT_STREQ(
      L"\"c:\\my path\\YesExe.exe\" arg1 arg2 arg3 --installerdata=\"c:\\my "
      L"path\\installer data file.dat\"",
      BuildExeCommandLine(
          std::wstring(L"arg1 arg2 arg3"),
          base::FilePath(L"c:\\my path\\installer data file.dat"),
          base::FilePath(L"c:\\my path\\YesExe.exe"))
          .c_str());
}

TEST(WinUtil, ShellExecuteAndWait) {
  DWORD exit_code = 0;

  EXPECT_EQ(ShellExecuteAndWait(base::FilePath(L"NonExistent.Exe"), {}, {},
                                &exit_code),
            HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  EXPECT_HRESULT_SUCCEEDED(ShellExecuteAndWait(
      GetTestProcessCommandLine(GetTestScope()).GetProgram(), {}, {},
      &exit_code));
  EXPECT_EQ(exit_code, 0UL);
}

TEST(WinUtil, RunElevated) {
  // TODO(crbug.com/1314521): Click on UAC prompts in Updater tests that require
  // elevation
  if (!::IsUserAnAdmin())
    return;

  DWORD exit_code = 0;
  const base::CommandLine test_process_cmd_line =
      GetTestProcessCommandLine(GetTestScope());
  EXPECT_HRESULT_SUCCEEDED(
      RunElevated(test_process_cmd_line.GetProgram(),
                  test_process_cmd_line.GetArgumentsString(), &exit_code));
  EXPECT_EQ(exit_code, 0UL);
}

}  // namespace updater
