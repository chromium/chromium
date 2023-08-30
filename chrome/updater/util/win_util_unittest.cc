// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/win_util.h"

#include <regstr.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

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
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/unit_test_util_win.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

constexpr char kTestAppID[] = "{D07D2B56-F583-4631-9E8E-9942F63765BE}";

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
  EXPECT_THAT(ShellExecuteAndWait(base::FilePath(L"NonExistent.Exe"), {}, {}),
              base::test::ErrorIs(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)));

  EXPECT_THAT(ShellExecuteAndWait(
                  GetTestProcessCommandLine(GetTestScope(), test::GetTestName())
                      .GetProgram(),
                  {}, {}),
              base::test::ValueIs(DWORD{0}));
}

TEST(WinUtil, RunElevated) {
  // TODO(crbug.com/1314521): Click on UAC prompts in Updater tests that require
  // elevation
  if (!::IsUserAnAdmin())
    return;

  const base::CommandLine test_process_cmd_line =
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName());
  EXPECT_THAT(RunElevated(test_process_cmd_line.GetProgram(),
                          test_process_cmd_line.GetArgumentsString()),
              base::test::ValueIs(DWORD{0}));
}

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

  EXPECT_TRUE(test::WaitFor([&]() {
    return test::FindProcesses(kTestProcessExecutableName).empty();
  }));
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
  EXPECT_THAT(IsCOMCallerAdmin(), base::test::ValueIs(::IsUserAnAdmin()));
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

TEST(WinUtil, StopProcessesUnderPath) {
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  exe_dir = exe_dir.AppendASCII(test::GetTestName());

  base::CommandLine command_line =
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName());
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
  std::string ap(GetAppAPValue(GetTestScope(), kTestAppID));
  EXPECT_EQ(ap, "");

  base::win::RegKey client_state_key(
      CreateAppClientStateKey(GetTestScope(), base::ASCIIToWide(kTestAppID)));
  EXPECT_EQ(client_state_key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);

  ap = GetAppAPValue(GetTestScope(), kTestAppID);
  EXPECT_EQ(ap, "TestAP");

  DeleteAppClientStateKey(GetTestScope(), base::ASCIIToWide(kTestAppID));
}

}  // namespace updater
