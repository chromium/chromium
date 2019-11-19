// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/crash/crash_client.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/secure_dll_loading.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace {

bool IsSandboxedProcess() {
  static const bool is_sandboxed_process =
      (sandbox::SandboxFactory::GetTargetServices() != nullptr);
  return is_sandboxed_process;
}

// base::TestSuite's Initialize method initializes logging differently than we
// do. This subclass ensures logging is properly initialized using ScopedLogging
// after base::TestSuite::Initialize has run.
class ChromeCleanerTestSuite : public base::TestSuite {
 public:
  // Inherit constructors.
  using base::TestSuite::TestSuite;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    scoped_logging.reset(new chrome_cleaner::ScopedLogging(
        IsSandboxedProcess() ? chrome_cleaner::kSandboxLogFileSuffix
                             : nullptr));
  }

 private:
  std::unique_ptr<chrome_cleaner::ScopedLogging> scoped_logging;
};

}  // namespace

int main(int argc, char** argv) {
  // This must be executed as soon as possible to reduce the number of dlls that
  // the code might try to load before we can lock things down.
  //
  // We enable secure DLL loading in the test suite to be sure that it doesn't
  // affect the behaviour of functionality that's tested.
  chrome_cleaner::EnableSecureDllLoading();

  ChromeCleanerTestSuite test_suite(argc, argv);

  if (!chrome_cleaner::SetupTestConfigs())
    return 1;

  if (!chrome_cleaner::CheckTestPrivileges())
    return 1;

  // Make sure tests will not end up in an infinite reboot loop.
  if (chrome_cleaner::Rebooter::IsPostReboot())
    return 0;

#if BUILDFLAG(IS_INTERNAL_CHROME_CLEANER_BUILD)
  // The tests will run with the internal engine, which takes longer.
  // IS_INTERNAL_CHROME_CLEANER_BUILD is only set on the Chrome Cleaner
  // builders, not the chromium builders, so this will not slow down the
  // general commit queue.
  constexpr base::TimeDelta kInternalTimeout = base::TimeDelta::FromMinutes(10);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestLauncherTimeout,
      base::NumberToString(kInternalTimeout.InMilliseconds()));
#endif

  // ScopedCOMInitializer keeps COM initialized in a specific scope. We don't
  // want to initialize it for sandboxed processes, so manage its lifetime with
  // a unique_ptr, which will call ScopedCOMInitializer's destructor when it
  // goes out of scope below.
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer;

  if (!IsSandboxedProcess()) {
    scoped_com_initializer = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    bool success = chrome_cleaner::InitializeCOMSecurity();
    DCHECK(success) << "InitializeCOMSecurity() failed.";

    success = chrome_cleaner::TaskScheduler::Initialize();
    DCHECK(success) << "TaskScheduler::Initialize() failed.";

    // Crash reporting must be initialized only once, so it cannot be
    // initialized by individual tests or fixtures. Also, since crashpad does
    // not actually enable uploading of crash reports in non-official builds
    // (unless forced to by the --enable-crash-reporting flag) we don't need to
    // disable crash reporting.
    chrome_cleaner::CrashClient::GetInstance()->InitializeCrashReporting(
        chrome_cleaner::CrashClient::Mode::CLEANER,
        chrome_cleaner::SandboxType::kNonSandboxed);
  }

  // Some tests spawn sandbox targets using job objects. Windows 7 doesn't
  // support nested job objects, so don't use them in the test suite. Otherwise
  // all sandbox tests will fail as they try to create a second job object.
  bool use_job_objects = base::win::GetVersion() >= base::win::Version::WIN8;

  // Some tests will fail if two tests try to launch test_process.exe
  // simultaneously, so run the tests serially. This will still shard them and
  // distribute the shards to different swarming bots, but tests will run
  // serially on each bot.
  const int result = base::LaunchUnitTestsWithOptions(
      argc, argv,
      /*parallel_jobs=*/1U,        // Like LaunchUnitTestsSerially
      /*default_batch_limit=*/10,  // Like LaunchUnitTestsSerially
      use_job_objects,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  if (!IsSandboxedProcess())
    chrome_cleaner::TaskScheduler::Terminate();

  return result;
}
