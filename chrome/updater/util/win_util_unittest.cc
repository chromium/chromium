// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/win_util.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/unittest_util_win.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

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
  HResultOr<DWORD> result =
      ShellExecuteAndWait(base::FilePath(L"NonExistent.Exe"), {}, {});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  result = ShellExecuteAndWait(
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName())
          .GetProgram(),
      {}, {});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), DWORD{0});
}

TEST(WinUtil, RunElevated) {
  // TODO(crbug.com/1314521): Click on UAC prompts in Updater tests that require
  // elevation
  if (!::IsUserAnAdmin())
    return;

  const base::CommandLine test_process_cmd_line =
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName());
  HResultOr<DWORD> result =
      RunElevated(test_process_cmd_line.GetProgram(),
                  test_process_cmd_line.GetArgumentsString());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), DWORD{0});
}

namespace {

// Allows access to all authenticated users on the machine.
CSecurityDesc GetEveryoneDaclSecurityDescriptor(ACCESS_MASK accessmask) {
  CSecurityDesc sd;
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);
  dacl.AddAllowedAce(Sids::Interactive(), accessmask);

  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  return sd;
}

}  // namespace

TEST(WinUtil, RunDeElevated_Exe) {
  if (!::IsUserAnAdmin() || !IsUACOn())
    return;

  // Create a shared event to be waited for in this process and signaled in the
  // test process to confirm that the test process is running at medium
  // integrity.
  // The event is created with a security descriptor that allows the medium
  // integrity process to signal it.
  const std::wstring event_name =
      base::StrCat({L"WinUtil.RunDeElevated-",
                    base::NumberToWString(::GetCurrentProcessId())});
  CSecurityAttributes sa(GetEveryoneDaclSecurityDescriptor(GENERIC_ALL));
  base::WaitableEvent event(base::win::ScopedHandle(
      ::CreateEvent(&sa, FALSE, FALSE, event_name.c_str())));
  ASSERT_NE(event.handle(), nullptr);

  base::CommandLine test_process_cmd_line =
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName());
  test_process_cmd_line.AppendSwitchNative(kTestEventToSignalIfMediumIntegrity,
                                           event_name);
  EXPECT_HRESULT_SUCCEEDED(
      RunDeElevated(test_process_cmd_line.GetProgram().value(),
                    test_process_cmd_line.GetArgumentsString()));
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));

  EXPECT_TRUE(test::WaitFor(base::BindLambdaForTesting([&]() {
    return test::FindProcesses(kTestProcessExecutableName).empty();
  })));
}

TEST(WinUtil, GetOSVersion) {
  absl::optional<OSVERSIONINFOEX> rtl_os_version = GetOSVersion();
  ASSERT_NE(rtl_os_version, absl::nullopt);

  // Compare to the version from `::GetVersionEx`.
  OSVERSIONINFOEX os = {};
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_TRUE(::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&os)));
#pragma clang diagnostic pop

  EXPECT_EQ(rtl_os_version->dwOSVersionInfoSize, os.dwOSVersionInfoSize);
  EXPECT_EQ(rtl_os_version->dwMajorVersion, os.dwMajorVersion);
  EXPECT_EQ(rtl_os_version->dwMinorVersion, os.dwMinorVersion);
  EXPECT_EQ(rtl_os_version->dwBuildNumber, os.dwBuildNumber);
  EXPECT_EQ(rtl_os_version->dwPlatformId, os.dwPlatformId);
  EXPECT_STREQ(rtl_os_version->szCSDVersion, os.szCSDVersion);
  EXPECT_EQ(rtl_os_version->wServicePackMajor, os.wServicePackMajor);
  EXPECT_EQ(rtl_os_version->wServicePackMinor, os.wServicePackMinor);
  EXPECT_EQ(rtl_os_version->wSuiteMask, os.wSuiteMask);
  EXPECT_EQ(rtl_os_version->wProductType, os.wProductType);
}

TEST(WinUtil, CompareOSVersions_SameAsCurrent) {
  absl::optional<OSVERSIONINFOEX> this_os = GetOSVersion();
  ASSERT_NE(this_os, absl::nullopt);

  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewBuildNumber) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwBuildNumber, 0UL);
  --prior_os->dwBuildNumber;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMajor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwMajorVersion, 0UL);
  --prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);

  // This test only runs if the current OS has a minor version.
  if (prior_os->dwMinorVersion >= 1) {
    --prior_os->dwMinorVersion;

    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
  }
}

TEST(WinUtil, CompareOSVersions_NewMajorWithLowerMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwMajorVersion, 0UL);
  --prior_os->dwMajorVersion;
  ++prior_os->dwMinorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_OldMajor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ++prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_OldMajorWithHigherMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);

  // This test only runs if the current OS has a minor version.
  if (prior_os->dwMinorVersion >= 1) {
    ++prior_os->dwMajorVersion;
    --prior_os->dwMinorVersion;

    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
  }
}

TEST(WinUtil, IsCOMCallerAdmin) {
  HResultOr<bool> is_com_caller_admin = IsCOMCallerAdmin();
  ASSERT_TRUE(is_com_caller_admin.has_value());
  EXPECT_EQ(is_com_caller_admin.value(), ::IsUserAnAdmin());
}

TEST(WinUtil, EnableSecureDllLoading) {
  EXPECT_TRUE(EnableSecureDllLoading());
}

TEST(WinUtil, EnableProcessHeapMetadataProtection) {
  EXPECT_TRUE(EnableProcessHeapMetadataProtection());
}

TEST(WinUtil, CreateSecureTempDir) {
  absl::optional<base::ScopedTempDir> temp_dir = CreateSecureTempDir();
  EXPECT_TRUE(temp_dir);
  EXPECT_TRUE(temp_dir->IsValid());

  base::FilePath program_files_dir;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));
  EXPECT_EQ(program_files_dir.IsParent(temp_dir->GetPath()),
            !!::IsUserAnAdmin());
}

TEST(WinUtil, SignalShutdownEvent) {
  {
    const base::ScopedClosureRunner reset_shutdown_event(
        SignalShutdownEvent(GetTestScope()));

    // Expect that the legacy GoogleUpdate shutdown event is signaled.
    EXPECT_TRUE(IsShutdownEventSignaled(GetTestScope()))
        << "Unexpected shutdown event not signaled";
  }

  // Expect that the legacy GoogleUpdate shutdown event is invalid now.
  EXPECT_FALSE(IsShutdownEventSignaled(GetTestScope()))
      << "Unexpected shutdown event signaled";
}

TEST(WinUtil, StopGoogleUpdateProcesses) {
  // TODO(crbug.com/1290496) perhaps some comprehensive tests for
  // `StopGoogleUpdateProcesses`?
  EXPECT_TRUE(StopGoogleUpdateProcesses(GetTestScope()));
}

TEST(WinUtil, IsGuid) {
  EXPECT_FALSE(IsGuid(L"c:\\test\\dir"));
  EXPECT_FALSE(IsGuid(L"a"));
  EXPECT_FALSE(IsGuid(L"CA3045BFA6B14fb8A0EFA615CEFE452C"));

  // Missing {}.
  EXPECT_FALSE(IsGuid(L"CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C"));

  // Invalid char X.
  EXPECT_FALSE(IsGuid(L"{XA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}"));

  // Invalid binary char 0x200.
  EXPECT_FALSE(IsGuid(L"{\0x200a3045bf-a6b1-4fb8-a0ef-a615cefe452c}"));

  // Missing -.
  EXPECT_FALSE(IsGuid(L"{CA3045BFA6B14fb8A0EFA615CEFE452C}"));

  // Double quotes.
  EXPECT_FALSE(IsGuid(L"\"{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}\""));

  EXPECT_TRUE(IsGuid(L"{CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}"));
  EXPECT_TRUE(IsGuid(L"{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}"));
}

}  // namespace updater
