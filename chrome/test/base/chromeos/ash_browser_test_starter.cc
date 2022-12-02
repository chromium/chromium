// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash_browser_test_starter.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
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
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
       ash::features::kLacrosOnly},
      {});
  command_line->AppendSwitch(ash::switches::kAshEnableWaylandServer);
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
