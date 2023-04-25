// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash_browser_test_starter.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/views_switches.h"

namespace test {

AshBrowserTestStarter::AshBrowserTestStarter() = default;
AshBrowserTestStarter::~AshBrowserTestStarter() = default;

bool AshBrowserTestStarter::HasLacrosArgument() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kLacrosChromePath);
}

bool AshBrowserTestStarter::PrepareEnvironmentForLacros() {
  DCHECK(HasLacrosArgument());
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!scoped_temp_dir_xdg_.CreateUniqueTempDir()) {
    return false;
  }
  env->SetVar("XDG_RUNTIME_DIR", scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Put lacros logs in CAS outputs on bots.
  if (command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
    std::string test_name = base::JoinString(
        {::testing::UnitTest::GetInstance()
             ->current_test_info()
             ->test_suite_name(),
         ::testing::UnitTest::GetInstance()->current_test_info()->name()},
        ".");
    base::FilePath output_file_path =
        command_line->GetSwitchValuePath(switches::kTestLauncherSummaryOutput);
    base::FilePath test_output_folder =
        output_file_path.DirName().Append(test_name);
    // Handle retry logic. When a test fail and retry, we need to use a new
    // folder for user data dir.
    int retry_count = 1;
    while (base::PathExists(test_output_folder) && retry_count < 5) {
      test_output_folder = output_file_path.DirName().Append(
          test_name + ".retry_" + base::NumberToString(retry_count));
      ++retry_count;
    }
    // Unlikely the path still exist. But in case it happens, we would let
    // the browser test framework to create the tmp folder as usual.
    if (!base::PathExists(test_output_folder)) {
      command_line->AppendSwitchPath(switches::kUserDataDir,
                                     test_output_folder);
    }
  } else {
    LOG(WARNING)
        << "By default, lacros logs are in some random folder. If you need "
        << "lacros log, please run with --test-launcher-summary-output. e.g. "
        << "Run with --test-launcher-summary-output=/tmp/default/output.json. "
        << "For each lacros in a test, the log will be in "
        << "/tmp/default/test_suite.test_name folder.";
  }

  scoped_feature_list_.InitWithFeatures(
      {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
       ash::features::kLacrosOnly},
      {});
  command_line->AppendSwitch(ash::switches::kAshEnableWaylandServer);
  command_line->AppendSwitch(
      views::switches::kDisableInputEventActivationProtectionForTesting);
  command_line->AppendSwitch(ash::switches::kDisableLacrosKeepAliveForTesting);
  command_line->AppendSwitch(ash::switches::kDisableLoginLacrosOpening);
  command_line->AppendSwitch(switches::kNoStartupWindow);
  command_line->AppendSwitchASCII(ash::switches::kLacrosChromeAdditionalArgs,
                                  "--no-first-run");
  return true;
}

class LacrosStartedObserver : public crosapi::BrowserManagerObserver {
 public:
  LacrosStartedObserver() = default;
  LacrosStartedObserver(const LacrosStartedObserver&) = delete;
  LacrosStartedObserver& operator=(const LacrosStartedObserver&) = delete;
  ~LacrosStartedObserver() override = default;

  void OnStateChanged() override {
    if (crosapi::BrowserManager::Get()->IsRunning()) {
      run_loop_.Quit();
    }
  }

  void Wait(base::TimeDelta timeout) {
    if (crosapi::BrowserManager::Get()->IsRunning()) {
      return;
    }
    base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop_.QuitClosure(),
                                      timeout);
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
};

void WaitForExoStarted(const base::FilePath& xdg_path) {
  base::RepeatingTimer timer;
  base::RunLoop run_loop;
  timer.Start(FROM_HERE, base::Seconds(1), base::BindLambdaForTesting([&]() {
                if (base::PathExists(xdg_path.Append("wayland-0")) &&
                    base::PathExists(xdg_path.Append("wayland-0.lock"))) {
                  run_loop.Quit();
                }
              }));
  base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                    TestTimeouts::action_max_timeout());
  run_loop.Run();
  CHECK(base::PathExists(xdg_path.Append("wayland-0")) &&
        base::PathExists(xdg_path.Append("wayland-0.lock")));
}

void AshBrowserTestStarter::StartLacros(InProcessBrowserTest* test_class_obj) {
  DCHECK(HasLacrosArgument());

  WaitForExoStarted(scoped_temp_dir_xdg_.GetPath());

  crosapi::BrowserManager::Get()->NewWindow(
      /*incongnito=*/false, /*should_trigger_session_restore=*/false);

  LacrosStartedObserver observer;
  crosapi::BrowserManager::Get()->AddObserver(&observer);
  observer.Wait(TestTimeouts::action_max_timeout());
  crosapi::BrowserManager::Get()->RemoveObserver(&observer);

  CHECK(crosapi::BrowserManager::Get()->IsRunning());

  // Create a new ash browser window so browser() can work.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  chrome::NewEmptyWindow(profile);
  test_class_obj->SelectFirstBrowser();
}

}  // namespace test
