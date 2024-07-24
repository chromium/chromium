// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

namespace {

// A dir on DUT to host wayland socket and arc-bridge sockets.
inline constexpr char kRunChrome[] = "/run/chrome";

}  // namespace

AshIntegrationTest::AshIntegrationTest() = default;

AshIntegrationTest::~AshIntegrationTest() = default;

void AshIntegrationTest::WaitForAshFullyStarted() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableWaylandServer))
      << "Wayland server should be enabled.";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath xdg_path(kRunChrome);
  base::RepeatingTimer timer;
  base::RunLoop run_loop1;
  timer.Start(FROM_HERE, base::Milliseconds(100),
              base::BindLambdaForTesting([&]() {
                if (base::PathExists(xdg_path.Append("wayland-0")) &&
                    base::PathExists(xdg_path.Append("wayland-0.lock"))) {
                  run_loop1.Quit();
                }
              }));
  base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop1.QuitClosure(),
                                    TestTimeouts::action_max_timeout());
  run_loop1.Run();
  CHECK(base::PathExists(xdg_path.Append("wayland-0")));
  CHECK(base::PathExists(xdg_path.Append("wayland-0.lock")));

  // Wait for ChromeBrowserMainExtraParts::PostBrowserStart() to execute so that
  // crosapi is initialized.
  auto* extra_parts = ChromeBrowserMainExtraPartsAsh::Get();
  CHECK(extra_parts);
  if (!extra_parts->did_post_browser_start()) {
    base::RunLoop run_loop2;
    extra_parts->set_post_browser_start_callback(run_loop2.QuitClosure());
    run_loop2.Run();
  }
  CHECK(extra_parts->did_post_browser_start());
}

void AshIntegrationTest::SetUpCommandLine(base::CommandLine* command_line) {
  InteractiveAshTest::SetUpCommandLine(command_line);

  // Enable the Wayland server.
  command_line->AppendSwitch(ash::switches::kAshEnableWaylandServer);

  // Set up XDG_RUNTIME_DIR for Wayland.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("XDG_RUNTIME_DIR", kRunChrome);
}
