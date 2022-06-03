// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/fallback_crash_handling_win.h"

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace crash_reporter {

namespace {

const DWORD kExceptionCode = 0xCAFEBABE;

// This main function runs in two modes, first as a faux-crashpad handler,
// and then with --type=fallback-handler to handle the crash in the first
// instance.
MULTIPROCESS_TEST_MAIN(FallbackCrashHandlingWinRunHandler) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->GetSwitchValueASCII("type") ==
      switches::kFallbackCrashHandler) {
    return RunAsFallbackCrashHandler(*cmd_line,
                                     "FallbackCrashHandlingWinRunHandler",
                                     "1.2.3.4", "FakeChannel");
  }

  CHECK(SetupFallbackCrashHandling(*cmd_line));

  // Provoke a crash with a well-defined exception code.
  // The process shouldn't terminate with this exception code.
  RaiseException(kExceptionCode, 0, 0, nullptr);

  // This process should never return from the exception.
  CHECK(false) << "Unexpected return from RaiseException";

  // Should never get here.
  return 0;
}

class FallbackCrashHandlingTest : public base::MultiProcessTest {
 public:
  void SetUp() override { ASSERT_TRUE(database_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir database_dir_;
};

}  // namespace

TEST_F(FallbackCrashHandlingTest, SetupAndRunAsFallbackCrashHandler) {
  // Launch a subprocess to test the fallback handling implementation.
  base::CommandLine cmd_line = base::GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchPath("database", database_dir_.GetPath());

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process test_child = base::SpawnMultiProcessTestChild(
      "FallbackCrashHandlingWinRunHandler", cmd_line, options);

  ASSERT_TRUE(test_child.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(test_child.WaitForExit(&exit_code));
  ASSERT_EQ(kFallbackCrashTerminationCode, static_cast<uint32_t>(exit_code));

  // Validate that the database contains one valid crash dump.
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(
          database_dir_.GetPath());

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  ASSERT_EQ(crashpad::CrashReportDatabase::kNoError,
            database->GetPendingReports(&reports));

  EXPECT_EQ(1U, reports.size());
}

}  // namespace crash_reporter
