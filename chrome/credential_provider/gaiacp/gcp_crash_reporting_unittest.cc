// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "components/crash/core/app/crashpad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace credential_provider {

namespace {

// Switches passed to the child process below.
constexpr char kDllPathSwitch[] = "gcpw-dll-path";
constexpr char kCrashDatabaseSwitch[] = "gcpw-crash-database";

// Exit codes of the child process.
constexpr int kChildBadArguments = 3;
constexpr int kChildHandlerDidNotStart = 4;

}  // namespace

// Starts the crashpad handler like ConfigureGcpCrashReporting() does, for the
// DLL and crash database given on the command line, and asks it for a report.
// Runs in a child process because the handler exits only when its client does.
MULTIPROCESS_TEST_MAIN(RunDllCrashpadHandlerClient) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const base::FilePath dll_path =
      command_line->GetSwitchValuePath(kDllPathSwitch);
  const base::FilePath database =
      command_line->GetSwitchValuePath(kCrashDatabaseSwitch);
  if (dll_path.empty() || database.empty()) {
    return kChildBadArguments;
  }

  base::CommandLine handler_cmd_line(base::CommandLine::NO_PROGRAM);
  if (FAILED(GetCommandLineForCrashpadHandler(dll_path, &handler_cmd_line))) {
    return kChildBadArguments;
  }

  // Intentionally leaked, as in ConfigureGcpCrashReporting().
  crash_reporter::SetCrashReporterClient(new GcpCrashReporterClient(database));

  // Fails if the handler exits before it is ready, e.g. if rundll32 cannot
  // parse its command line.
  if (!crash_reporter::InitializeCrashpadWithDllEmbeddedHandler(
          /*initial_client=*/true, "GCPW DLL", /*user_data_dir=*/"",
          handler_cmd_line.GetProgram(),
          {base::WideToUTF8(handler_cmd_line.GetArgs()[0])})) {
    return kChildHandlerDidNotStart;
  }

  // Returns once the handler has written the report to the database.
  crash_reporter::DumpWithoutCrashing();
  return 0;
}

// Verifies that the crashpad handler of the DLL in the build directory starts
// through rundll32 and writes reports. Fails if rundll32 cannot parse the
// argument from GetCommandLineForCrashpadHandler() after crashpad quotes it, or
// if RunAsCrashpadHandlerW() does not remove it from the handler's arguments.
TEST(GcpCrashReportingTest, HandlerStartsThroughRunDll32AndWritesReport) {
#if defined(COMPONENT_BUILD)
  GTEST_SKIP() << "rundll32 cannot load a component build of the DLL.";
#else
  constexpr base::FilePath::CharType kCredentialProviderDll[] =
      FILE_PATH_LITERAL("gaia1_0.dll");

  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  const base::FilePath dll_path = exe_dir.Append(kCredentialProviderDll);
  if (!base::PathExists(dll_path)) {
    GTEST_SKIP() << dll_path << " not found.";
  }

  // The handler cannot start if the DLL path has spaces, see gcp_utils.h.
  base::CommandLine handler_cmd_line(base::CommandLine::NO_PROGRAM);
  ASSERT_EQ(S_OK,
            GetCommandLineForCrashpadHandler(dll_path, &handler_cmd_line));
  const std::wstring entrypoint_arg = handler_cmd_line.GetArgs()[0];
  if (entrypoint_arg.find(L' ') != std::wstring::npos) {
    GTEST_SKIP() << "8.3 names are disabled on this volume: " << entrypoint_arg;
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath database =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("Crashpad"));

  base::CommandLine child_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  child_command_line.AppendSwitchPath(kDllPathSwitch, dll_path);
  child_command_line.AppendSwitchPath(kCrashDatabaseSwitch, database);
  base::Process child = base::SpawnMultiProcessTestChild(
      "RunDllCrashpadHandlerClient", child_command_line, base::LaunchOptions());
  ASSERT_TRUE(child.IsValid());

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_max_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);

  // The handler creates the database and writes reports into "reports".
  EXPECT_TRUE(
      base::PathExists(database.Append(FILE_PATH_LITERAL("settings.dat"))));
  base::FileEnumerator reports(database.Append(FILE_PATH_LITERAL("reports")),
                               /*recursive=*/false, base::FileEnumerator::FILES,
                               FILE_PATH_LITERAL("*.dmp"));
  EXPECT_FALSE(reports.Next().empty());

  // The handler exits after the child. Wait for it to release the database.
  for (int i = 0; i < 50 && !base::DeletePathRecursively(temp_dir.GetPath());
       ++i) {
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }
#endif  // defined(COMPONENT_BUILD)
}

}  // namespace credential_provider
