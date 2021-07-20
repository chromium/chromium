// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash_browser_test_starter.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

AshBrowserTestStarter::AshBrowserTestStarter() = default;
AshBrowserTestStarter::~AshBrowserTestStarter() = default;

bool AshBrowserTestStarter::HasLacrosArgument() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kLacrosChromePath);
}

bool AshBrowserTestStarter::PrepareEnvironmentForLacros() {
  DCHECK(HasLacrosArgument());
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!scoped_temp_dir_xdg_.CreateUniqueTempDir()) {
    return false;
  }
  env->SetVar("XDG_RUNTIME_DIR", scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());
  if (!scoped_temp_dir_ash_.CreateUniqueTempDir()) {
    return false;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchPath("user-data-dir",
                                 scoped_temp_dir_ash_.GetPath());

  scoped_feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  command_line->AppendSwitch("enable-wayland-server");
  command_line->AppendSwitch("no-startup-window");
  return true;
}

class LacrosStartedObserver : public crosapi::BrowserManagerObserver {
 public:
  explicit LacrosStartedObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  LacrosStartedObserver(const LacrosStartedObserver&) = delete;
  LacrosStartedObserver& operator=(const LacrosStartedObserver&) = delete;
  ~LacrosStartedObserver() override = default;

  void OnStateChanged() override {
    if (crosapi::BrowserManager::Get()->IsRunning()) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

void AshBrowserTestStarter::StartLacros() {
  DCHECK(HasLacrosArgument());
  crosapi::BrowserManager::Get()->NewWindow(/*incongnito=*/false);
  base::RunLoop run_loop;
  LacrosStartedObserver observer(run_loop.QuitClosure());
  crosapi::BrowserManager::Get()->AddObserver(&observer);
  base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                    TestTimeouts::action_max_timeout());
  run_loop.Run();
  crosapi::BrowserManager::Get()->RemoveObserver(&observer);
  CHECK(crosapi::BrowserManager::Get()->IsRunning());
}

}  // namespace test
