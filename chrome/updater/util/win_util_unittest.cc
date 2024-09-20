// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/win_util.h"

#include <objbase.h>

#include <windows.h>

#include <regstr.h>
#include <shellapi.h>
#include <shlobj.h>

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/uuid.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test {

namespace {

constexpr char kTestAppID[] = "{D07D2B56-F583-4631-9E8E-9942F63765BE}";

}  // namespace

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
  EXPECT_THAT(ShellExecuteAndWait(base::FilePath(L"NonExistent.Exe"), {}, {}),
              base::test::ErrorIs(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)));

  EXPECT_THAT(
      ShellExecuteAndWait(GetTestProcessCommandLine(GetUpdaterScopeForTesting(),
                                                    test::GetTestName())
                              .GetProgram(),
                          {}, {}),
      base::test::ValueIs(DWORD{0}));
}

TEST(WinUtil, RunElevated) {
  if (!::IsUserAnAdmin()) {
    return;
  }
  const base::CommandLine test_process_cmd_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());
  EXPECT_THAT(RunElevated(test_process_cmd_line.GetProgram(),
                          test_process_cmd_line.GetArgumentsString()),
              base::test::ValueIs(DWORD{0}));
}

TEST(WinUtil, RunDeElevatedCmdLine_Exe) {
  // Create a shared event to be waited for in this process and signaled in the
  // test process to confirm that the test process is running at medium
  // integrity.
  test::EventHolder event_holder(IsElevatedWithUACOn()
                                     ? CreateEveryoneWaitableEventForTest()
                                     : test::CreateWaitableEventForTest());
  ASSERT_NE(event_holder.event.handle(), nullptr);

  base::CommandLine test_process_cmd_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());
  test_process_cmd_line.AppendSwitchNative(
      IsElevatedWithUACOn() ? kTestEventToSignalIfMediumIntegrity
                            : kTestEventToSignal,
      event_holder.name);
  EXPECT_HRESULT_SUCCEEDED(
      RunDeElevatedCmdLine(test_process_cmd_line.GetCommandLineString()));
  EXPECT_TRUE(event_holder.event.TimedWait(TestTimeouts::action_max_timeout()));

  EXPECT_TRUE(test::WaitFor(
      [] { return test::FindProcesses(kTestProcessExecutableName).empty(); }));
}

TEST(WinUtil, GetOSVersion) {
  std::optional<OSVERSIONINFOEX> rtl_os_version = GetOSVersion();
  ASSERT_NE(rtl_os_version, std::nullopt);

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
  std::optional<OSVERSIONINFOEX> this_os = GetOSVersion();
  ASSERT_NE(this_os, std::nullopt);

  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewBuildNumber) {
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);
  ASSERT_GT(prior_os->dwBuildNumber, 0UL);
  --prior_os->dwBuildNumber;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMajor) {
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);
  ASSERT_GT(prior_os->dwMajorVersion, 0UL);
  --prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMinor) {
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);

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
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);
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
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);
  ++prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_OldMajorWithHigherMinor) {
  std::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, std::nullopt);

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
  EXPECT_THAT(IsCOMCallerAdmin(), base::test::ValueIs(::IsUserAnAdmin()));
}

TEST(WinUtil, EnableSecureDllLoading) {
  EXPECT_TRUE(EnableSecureDllLoading());
}

TEST(WinUtil, EnableProcessHeapMetadataProtection) {
  EXPECT_TRUE(EnableProcessHeapMetadataProtection());
}

TEST(WinUtil, CreateSecureTempDir) {
  std::optional<base::ScopedTempDir> temp_dir = CreateSecureTempDir();
  EXPECT_TRUE(temp_dir);
  EXPECT_TRUE(temp_dir->IsValid());
}

TEST(WinUtil, SignalShutdownEvent) {
  {
    const base::ScopedClosureRunner reset_shutdown_event(
        SignalShutdownEvent(GetUpdaterScopeForTesting()));

    // Expect that the legacy GoogleUpdate shutdown event is signaled.
    EXPECT_TRUE(IsShutdownEventSignaled(GetUpdaterScopeForTesting()))
        << "Unexpected shutdown event not signaled";
  }

  // Expect that the legacy GoogleUpdate shutdown event is invalid now.
  EXPECT_FALSE(IsShutdownEventSignaled(GetUpdaterScopeForTesting()))
      << "Unexpected shutdown event signaled";
}

TEST(WinUtil, StopProcessesUnderPath) {
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  exe_dir = exe_dir.AppendASCII(test::GetTestName());

  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());
  command_line.AppendSwitchASCII(
      updater::kTestSleepSecondsSwitch,
      base::NumberToString(TestTimeouts::action_timeout().InSeconds() / 4));

  std::vector<base::Process> processes;
  for (const base::FilePath& dir :
       {exe_dir, exe_dir.Append(L"1"), exe_dir.Append(L"2")}) {
    ASSERT_TRUE(base::CreateDirectory(dir));

    for (const std::wstring exe_name : {L"random1.exe", L"random2.exe"}) {
      const base::FilePath exe(dir.Append(exe_name));
      ASSERT_TRUE(base::CopyFile(command_line.GetProgram(), exe));

      base::Process process = base::LaunchProcess(
          base::StrCat(
              {base::CommandLine::QuoteForCommandLineToArgvW(exe.value()), L" ",
               command_line.GetArgumentsString()}),
          {});
      ASSERT_TRUE(process.IsValid());
      processes.push_back(std::move(process));
    }
  }

  StopProcessesUnderPath(exe_dir, TestTimeouts::action_timeout());
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  for (const base::Process& process : processes) {
    EXPECT_FALSE(process.IsRunning()) << process.Pid();
  }

  EXPECT_TRUE(base::DeletePathRecursively(exe_dir));
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

TEST(WinUtil, ForEachRegistryRunValueWithPrefix) {
  constexpr int kRunEntries = 6;
  const std::wstring kRunEntryPrefix(base::ASCIIToWide(test::GetTestName()));

  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_READ | KEY_WRITE),
            ERROR_SUCCESS);

  for (int count = 0; count < kRunEntries; ++count) {
    std::wstring entry_name(kRunEntryPrefix);
    entry_name.push_back(L'0' + count);
    ASSERT_EQ(key.WriteValue(entry_name.c_str(), entry_name.c_str()),
              ERROR_SUCCESS);
  }

  int count_entries = 0;
  ForEachRegistryRunValueWithPrefix(
      kRunEntryPrefix,
      [&key, &count_entries, kRunEntryPrefix](const std::wstring& run_name) {
        EXPECT_TRUE(base::StartsWith(run_name, kRunEntryPrefix));
        ++count_entries;
        EXPECT_EQ(key.DeleteValue(run_name.c_str()), ERROR_SUCCESS);
      });
  EXPECT_EQ(count_entries, kRunEntries);
}

TEST(WinUtil, DeleteRegValue) {
  constexpr int kRegValues = 6;
  const std::wstring kRegValuePrefix(base::ASCIIToWide(test::GetTestName()));

  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_READ | KEY_WRITE),
            ERROR_SUCCESS);

  for (int count = 0; count < kRegValues; ++count) {
    std::wstring entry_name(kRegValuePrefix);
    entry_name.push_back(L'0' + count);
    ASSERT_EQ(key.WriteValue(entry_name.c_str(), entry_name.c_str()),
              ERROR_SUCCESS);

    EXPECT_TRUE(key.HasValue(entry_name.c_str()));
    EXPECT_TRUE(DeleteRegValue(HKEY_CURRENT_USER, REGSTR_PATH_RUN, entry_name));
    EXPECT_FALSE(key.HasValue(entry_name.c_str()));
    EXPECT_TRUE(DeleteRegValue(HKEY_CURRENT_USER, REGSTR_PATH_RUN, entry_name));
  }
}

TEST(WinUtil, ForEachServiceWithPrefix) {
  if (!::IsUserAnAdmin()) {
    return;
  }

  constexpr int kNumServices = 6;
  const std::wstring kServiceNamePrefix(base::ASCIIToWide(test::GetTestName()));

  for (int count = 0; count < kNumServices; ++count) {
    std::wstring service_name(kServiceNamePrefix);
    service_name.push_back(L'0' + count);
    EXPECT_TRUE(
        CreateService(service_name, service_name, L"C:\\temp\\temp.exe"));
  }

  int count_entries = 0;
  ForEachServiceWithPrefix(
      kServiceNamePrefix, kServiceNamePrefix,
      [&count_entries, kServiceNamePrefix](const std::wstring& service_name) {
        EXPECT_TRUE(base::StartsWith(service_name, kServiceNamePrefix));
        ++count_entries;
        EXPECT_TRUE(DeleteService(service_name));
      });
  EXPECT_EQ(count_entries, kNumServices);
}

TEST(WinUtil, DeleteService) {
  if (!::IsUserAnAdmin()) {
    return;
  }

  constexpr int kNumServices = 6;
  const std::wstring kServiceNamePrefix(base::ASCIIToWide(test::GetTestName()));

  for (int count = 0; count < kNumServices; ++count) {
    std::wstring service_name(kServiceNamePrefix);
    service_name.push_back(L'0' + count);
    ASSERT_TRUE(
        CreateService(service_name, service_name, L"C:\\temp\\temp.exe"));
    EXPECT_TRUE(DeleteService(service_name));
  }
}

TEST(WinUtil, LogClsidEntries) {
  CLSID clsid = {};
  EXPECT_HRESULT_SUCCEEDED(
      ::CLSIDFromProgID(L"InternetExplorer.Application", &clsid));
  LogClsidEntries(clsid);
}

TEST(WinUtil, GetAppAPValue) {
  std::string ap(GetAppAPValue(GetUpdaterScopeForTesting(), kTestAppID));
  EXPECT_EQ(ap, "");

  base::win::RegKey client_state_key(CreateAppClientStateKey(
      GetUpdaterScopeForTesting(), base::ASCIIToWide(kTestAppID)));
  EXPECT_EQ(client_state_key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);

  ap = GetAppAPValue(GetUpdaterScopeForTesting(), kTestAppID);
  EXPECT_EQ(ap, "TestAP");

  DeleteAppClientStateKey(GetUpdaterScopeForTesting(),
                          base::ASCIIToWide(kTestAppID));
}

struct WinUtilGetRegKeyContentsTestCase {
  const std::wstring reg_key;
  const std::wstring expected_substring;
};

class WinUtilGetRegKeyContentsTest
    : public ::testing::TestWithParam<WinUtilGetRegKeyContentsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    WinUtilGetRegKeyContentsTestCases,
    WinUtilGetRegKeyContentsTest,
    ::testing::ValuesIn(std::vector<WinUtilGetRegKeyContentsTestCase>{
        {L"HKLM\\SOFTWARE\\Classes\\CLSID\\{00020424-0000-0000-C000-"
         L"000000000046}",
         L"{00020424-0000-0000-C000-000000000046}"},
        {L"HKLM\\SOFTWARE\\WOW6432Node\\Classes\\CLSID\\{00020424-0000-0000-"
         L"C000-000000000046}",
         L"{00020424-0000-0000-C000-000000000046}"},
        {L"HKCR\\CLSID\\{00020424-0000-0000-C000-000000000046}",
         L"{00020424-0000-0000-C000-000000000046}"},
        {L"HKCR\\WOW6432Node\\CLSID\\{00020424-0000-0000-C000-000000000046}",
         L"{00020424-0000-0000-C000-000000000046}"},
    }));

TEST_P(WinUtilGetRegKeyContentsTest, TestCases) {
  std::optional<std::wstring> contents = GetRegKeyContents(GetParam().reg_key);
  ASSERT_TRUE(contents);
  ASSERT_NE(contents->find(GetParam().expected_substring), std::wstring::npos);
}

TEST(WinUtil, GetTextForSystemError) {
  EXPECT_EQ(GetTextForSystemError(2),
            L"The system cannot find the file specified. ");
  EXPECT_EQ(GetTextForSystemError(0x80070002),
            L"The system cannot find the file specified. ");
  EXPECT_EQ(GetTextForSystemError(12007),
            L"The server name or address could not be resolved ");
  EXPECT_EQ(GetTextForSystemError(0x80072ee7),
            L"The server name or address could not be resolved ");
  EXPECT_EQ(GetTextForSystemError(-2147012889),
            L"The server name or address could not be resolved ");
  EXPECT_EQ(
      GetTextForSystemError(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x200)),
      L"0x80040200");
}

TEST(WinUtil, GetLoggedOnUserToken) {
  if (!::IsUserAnAdmin() || !IsUACOn()) {
    return;
  }

  ASSERT_TRUE(::IsUserAnAdmin());
  HResultOr<ScopedKernelHANDLE> token = GetLoggedOnUserToken();
  ASSERT_TRUE(token.has_value());

  ScopedImpersonation impersonate;
  ASSERT_TRUE(SUCCEEDED(impersonate.Impersonate(token.value().get())));
  ASSERT_FALSE(::IsUserAnAdmin());
}

TEST(WinUtil, IsAuditMode) {
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP();
  }
  ASSERT_FALSE(IsAuditMode());
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .WriteValue(L"ImageState", L"IMAGE_STATE_UNDEPLOYABLE"),
            ERROR_SUCCESS);
  ASSERT_TRUE(IsAuditMode());
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .DeleteValue(L"ImageState"),
            ERROR_SUCCESS);
}

TEST(WinUtil, OemInstallState) {
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP();
  }
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .WriteValue(L"ImageState", L"IMAGE_STATE_UNDEPLOYABLE"),
            ERROR_SUCCESS);
  ASSERT_TRUE(SetOemInstallState());
  ASSERT_TRUE(IsOemInstalling());

  DWORD oem_install_time_minutes = 0;
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                        Wow6432(KEY_QUERY_VALUE))
          .ReadValueDW(kRegValueOemInstallTimeMin, &oem_install_time_minutes),
      ERROR_SUCCESS);

  // Rewind to 71 hours and 58 minutes before now.
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY, Wow6432(KEY_SET_VALUE))
          .WriteValue(
              kRegValueOemInstallTimeMin,
              (base::Minutes(oem_install_time_minutes + 2) - kMinOemModeTime)
                  .InMinutes()),
      ERROR_SUCCESS);
  ASSERT_TRUE(IsOemInstalling());

  // Rewind to 72 hours and 2 minutes before now.
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY, Wow6432(KEY_SET_VALUE))
          .WriteValue(
              kRegValueOemInstallTimeMin,
              (base::Minutes(oem_install_time_minutes - 2) - kMinOemModeTime)
                  .InMinutes()),
      ERROR_SUCCESS);
  ASSERT_FALSE(IsOemInstalling());

  ASSERT_TRUE(ResetOemInstallState());
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .DeleteValue(L"ImageState"),
            ERROR_SUCCESS);
}

TEST(WinUtil, StringFromGuid) {
  GUID guid = {0};
  EXPECT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));
  EXPECT_EQ(base::win::WStringFromGUID(guid), StringFromGuid(guid));
}

TEST(WinUtil, GetUniqueTempFilePath) {
  EXPECT_FALSE(GetUniqueTempFilePath({}));

  std::optional<base::FilePath> p = GetUniqueTempFilePath(base::FilePath(
      L"C:\\Program Files (x86)\\Google\\GoogleUpdater\\updater.log"));
  ASSERT_TRUE(p);
  std::wstring p_base = p->BaseName().value();
  EXPECT_TRUE(base::StartsWith(p_base, L"updater"));
  EXPECT_TRUE(base::EndsWith(p_base, L".log"));
  base::ReplaceSubstringsAfterOffset(&p_base, 0, L"updater", {});
  base::ReplaceSubstringsAfterOffset(&p_base, 0, L".log", {});
  EXPECT_TRUE(base::Uuid::ParseLowercase(base::WideToUTF8(p_base)).is_valid());
}

TEST(WinUtil, SetEulaAccepted) {
  // This will set `eulaaccepted=0` in the registry.
  EXPECT_TRUE(
      SetEulaAccepted(GetUpdaterScopeForTesting(), /*eula_accepted=*/false));
  DWORD eula_accepted = 0;
  const HKEY root = UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting());
  EXPECT_EQ(base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                .ReadValueDW(L"eulaaccepted", &eula_accepted),
            ERROR_SUCCESS);
  EXPECT_EQ(eula_accepted, 0ul);

  // This will delete the `eulaaccepted` value in the registry.
  EXPECT_TRUE(
      SetEulaAccepted(GetUpdaterScopeForTesting(), /*eula_accepted=*/true));
  EXPECT_FALSE(base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                   .HasValue(L"eulaaccepted"));
}

}  // namespace updater::test
