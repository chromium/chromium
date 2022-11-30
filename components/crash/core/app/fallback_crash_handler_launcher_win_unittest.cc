// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/fallback_crash_handler_launcher_win.h"

#include <dbghelp.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace crash_reporter {

namespace {

const wchar_t kFileName[] = L"crash.dmp";

// This function is called in the fallback handler process instance.
MULTIPROCESS_TEST_MAIN(FallbackCrashHandlerLauncherMain) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  // Check for presence of the "process" argument.
  CHECK(cmd_line->HasSwitch("process"));

  // Retrieve and check the handle to verify that it was inherited in.
  unsigned int uint_process = 0;
  CHECK(base::StringToUint(cmd_line->GetSwitchValueASCII("process"),
                           &uint_process));
  base::Process process(base::win::Uint32ToHandle(uint_process));

  // Verify that the process handle points to our parent.
  CHECK_EQ(base::GetParentProcessId(base::GetCurrentProcessHandle()),
           process.Pid());

  // Check the "thread" argument.
  CHECK(cmd_line->HasSwitch("thread"));

  // Retrieve the thread id.
  unsigned int thread_id = 0;
  CHECK(
      base::StringToUint(cmd_line->GetSwitchValueASCII("thread"), &thread_id));

  // Check the "exception-pointers" argument.
  CHECK(cmd_line->HasSwitch("exception-pointers"));
  uint64_t uint_exc_ptrs = 0;
  CHECK(base::StringToUint64(
      cmd_line->GetSwitchValueASCII("exception-pointers"), &uint_exc_ptrs));

  EXCEPTION_POINTERS* exc_ptrs = reinterpret_cast<EXCEPTION_POINTERS*>(
      static_cast<uintptr_t>(uint_exc_ptrs));

  // Check the "database" argument.
  CHECK(cmd_line->HasSwitch("database"));
  base::FilePath database_dir = cmd_line->GetSwitchValuePath("database");

  base::FilePath dump_path = database_dir.Append(kFileName);

  // Create a dump file in the directory.
  base::File dump(dump_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  CHECK(dump.IsValid());

  const MINIDUMP_TYPE kMinidumpType = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithHandleData | MiniDumpWithUnloadedModules |
      MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo |
      MiniDumpWithTokenInformation);

  MINIDUMP_EXCEPTION_INFORMATION exc_info = {};
  exc_info.ThreadId = thread_id;
  exc_info.ExceptionPointers = exc_ptrs;
  exc_info.ClientPointers = TRUE;  // ExceptionPointers in client.

  // Write the minidump as a direct test of the validity and permissions on the
  // parent process handle.
  if (!MiniDumpWriteDump(process.Handle(),        // Process handle.
                         process.Pid(),           // Process Id.
                         dump.GetPlatformFile(),  // File handle.
                         kMinidumpType,           // Minidump type.
                         &exc_info,               // Exception Param
                         nullptr,                 // UserStreamParam,
                         nullptr)) {              // CallbackParam
    DWORD error = GetLastError();

    dump.Close();
    CHECK(base::DeleteFile(dump_path));
    CHECK(false) << "Unable to write dump, error " << error;
  }

  return 0;
}

MULTIPROCESS_TEST_MAIN(TestCrashHandlerLauncherMain) {
  base::ScopedTempDir database_dir;
  CHECK(database_dir.CreateUniqueTempDir());

  // Construct the base command line that diverts to the main function above.
  base::CommandLine base_cmdline(
      base::GetMultiProcessTestChildBaseCommandLine());

  base_cmdline.AppendSwitchASCII(switches::kTestChildProcess,
                                 "FallbackCrashHandlerLauncherMain");

  FallbackCrashHandlerLauncher launcher;
  CHECK(launcher.Initialize(base_cmdline, database_dir.GetPath()));

  // Make like an access violation at the current place.
  EXCEPTION_RECORD exc_record = {};
  exc_record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  CONTEXT ctx = {};
  RtlCaptureContext(&ctx);
  CHECK_NE(0UL, ctx.ContextFlags);

  EXCEPTION_POINTERS exc_ptrs = {&exc_record, &ctx};

  CHECK_EQ(0UL, launcher.LaunchAndWaitForHandler(&exc_ptrs));

  // Check that the dump was written, e.g. it's an existing file with non-zero
  // size.
  base::File dump(database_dir.GetPath().Append(kFileName),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  CHECK(dump.IsValid());
  CHECK_NE(0, dump.GetLength());
  return 0;
}

class FallbackCrashHandlerLauncherTest : public base::MultiProcessTest {};

}  // namespace

TEST_F(FallbackCrashHandlerLauncherTest, LaunchAndWaitForHandler) {
  // Because this process is heavily multithreaded it's going to be flaky
  // and generally fraught with peril to try and grab a minidump of it.
  // Instead, fire off a sacrificial process to do the testing.
  base::Process test_process = SpawnChild("TestCrashHandlerLauncherMain");
  int exit_code = 0;
  ASSERT_TRUE(test_process.WaitForExit(&exit_code));
  ASSERT_EQ(0, exit_code);
}

}  // namespace crash_reporter
