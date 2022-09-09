// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/secure_dll_loading.h"

#include <windows.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/inheritable_event.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/test/child_process_logger.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class SecureDLLLoadingTest : public testing::TestWithParam<std::wstring> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(child_process_logger_.Initialize());
    base::FilePath out_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
    exe_path_ = out_dir.Append(GetParam() + L".exe");
    empty_dll_path_ = out_dir.Append(chrome_cleaner::kEmptyDll);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::Process LaunchProcess(bool disable_secure_dll_loading) {
    std::unique_ptr<base::WaitableEvent> init_done_notifier =
        chrome_cleaner::CreateInheritableEvent(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED);

    base::CommandLine command_line(exe_path_);
    command_line.AppendSwitchNative(
        chrome_cleaner::kInitDoneNotifierSwitch,
        base::NumberToWString(
            base::win::HandleToUint32(init_done_notifier->handle())));
    command_line.AppendSwitch(chrome_cleaner::kLoadEmptyDLLSwitch);

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
    if (disable_secure_dll_loading)
      command_line.AppendSwitch(chrome_cleaner::kAllowUnsecureDLLsSwitch);
#endif  // BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)

    // The default execution mode (ExecutionMode::kNone) is no longer supported
    // and displays an error dialog instead of trying to load the DLLs.
    command_line.AppendSwitchASCII(
        chrome_cleaner::kExecutionModeSwitch,
        base::NumberToString(
            static_cast<int>(chrome_cleaner::ExecutionMode::kCleanup)));

    chrome_cleaner::AppendTestSwitches(temp_dir_, &command_line);

    base::LaunchOptions options;
    options.handles_to_inherit.push_back(init_done_notifier->handle());
    child_process_logger_.UpdateLaunchOptions(&options);
    base::Process process = base::LaunchProcess(command_line, options);
    if (!process.IsValid()) {
      child_process_logger_.DumpLogs();
      return process;
    }

    // Make sure the process has finished its initialization (including loading
    // DLLs). Also check the process handle in case it exits with an error.
    std::vector<HANDLE> wait_handles{init_done_notifier->handle(),
                                     process.Handle()};
    DWORD wait_result = ::WaitForMultipleObjects(
        wait_handles.size(), wait_handles.data(), /*bWaitAll=*/false,
        TestTimeouts::action_max_timeout().InMilliseconds());
    // WAIT_OBJECT_0 is the first handle in the vector.
    if (wait_result == WAIT_OBJECT_0 + 1) {
      DWORD exit_code = 0;
      PLOG_IF(ERROR, !::GetExitCodeProcess(process.Handle(), &exit_code));
      ADD_FAILURE() << "Process exited with " << exit_code
                    << " before signalling init_done_notifier";
      child_process_logger_.DumpLogs();
    } else {
      EXPECT_EQ(wait_result, WAIT_OBJECT_0);
    }

    return process;
  }

  bool EmptyDLLLoaded(const base::Process& process) {
    std::set<std::wstring> module_paths;
    chrome_cleaner::GetLoadedModuleFileNames(process.Handle(), &module_paths);

    for (const auto& module_path : module_paths) {
      if (base::EqualsCaseInsensitiveASCII(empty_dll_path_.value(),
                                           module_path))
        return true;
    }
    return false;
  }

 private:
  chrome_cleaner::ChildProcessLogger child_process_logger_;
  base::FilePath exe_path_;
  base::FilePath empty_dll_path_;

  // Temp directory for child process log files.
  base::ScopedTempDir temp_dir_;
};

INSTANTIATE_TEST_SUITE_P(SecureDLLLoading,
                         SecureDLLLoadingTest,
                         // The value names cannot include ".exe" because "."
                         // is not a valid character in a test case name.
                         ::testing::Values(L"software_reporter_tool",
                                           L"chrome_cleanup_tool"),
                         chrome_cleaner::GetParamNameForTest());

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
TEST_P(SecureDLLLoadingTest, Disabled) {
  base::Process process = LaunchProcess(/*disable_secure_dll_loading=*/true);
  EXPECT_TRUE(EmptyDLLLoaded(process));

  // There is no need to finish running the process.
  EXPECT_TRUE(process.Terminate(0U, /*wait=*/true));
}
#endif  // BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)

TEST_P(SecureDLLLoadingTest, Default) {
  if (!::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                        "SetDefaultDllDirectories")) {
    // Skip this test if the SetDefaultDllDirectories function is unavailable
    // (this is normal on Windows 7 without update KB2533623.)
    return;
  }

  base::Process process = LaunchProcess(/*disable_secure_dll_loading=*/false);
  EXPECT_FALSE(EmptyDLLLoaded(process));

  // There is no need to finish running the process.
  EXPECT_TRUE(process.Terminate(0U, /*wait=*/true));
}
