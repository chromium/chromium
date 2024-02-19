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
#include "chromeos/ash/components/standalone_browser/test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace {

// A dir on DUT to host wayland socket and arc-bridge sockets.
inline constexpr char kRunChrome[] = "/run/chrome";

// Simulates a failure for a Gaia URL request.
std::unique_ptr<net::test_server::HttpResponse> HandleGaiaURL(
    const net::test_server::HttpRequest& request) {
  return std::make_unique<net::test_server::HungResponse>();
}

}  // namespace

AshIntegrationTest::AshIntegrationTest() = default;

AshIntegrationTest::~AshIntegrationTest() = default;

void AshIntegrationTest::SetUpCommandLineForLacros(
    base::CommandLine* command_line) {
  CHECK(command_line);

  OverrideGaiaUrlForLacros(command_line);
}

void AshIntegrationTest::SetUpLacrosBrowserManager() {
  // Skip the device owner lookup for Lacros. This mirrors what we do in ash
  // browser tests. See AshBrowserTestStarter::SetUpBrowserManager(). This
  // function does not use crosapi::FakeDeviceOwnershipWaiter because if the
  // test uses BrowserManager::Get() to install the fake waiter (e.g. in
  // SetUpOnMainThread) often the existing device ownership waiter has already
  // been called and its too late to install the fake.
  crosapi::BrowserManager::SkipDeviceOwnershipWaitForTesting(true);
}

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

void AshIntegrationTest::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();

  // The embedded test server starts accepting connections after fork.
  if (https_server_) {
    https_server_->StartAcceptingConnections();
  }
}

void AshIntegrationTest::OverrideGaiaUrlForLacros(
    base::CommandLine* command_line) {
  // When using real Gaia login, don't override the Gaia URL.
  if (login_mixin_.IsGaiaLoginMode()) {
    return;
  }

  // Set up an embedded test server.
  https_server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server_->RegisterRequestHandler(base::BindRepeating(&HandleGaiaURL));
  CHECK(https_server_->InitializeAndListen());

  std::vector<std::string> lacros_args;
  lacros_args.emplace_back(base::StringPrintf("--%s", switches::kNoFirstRun));
  // Override Gaia url in Lacros so that the gaia requests will NOT be handled
  // with the real internet connection, but with the embedded test server. The
  // embedded test server will simulate failure of the Gaia url requests which
  // is expected in testing environment for Gaia authentication flow. See
  // crbug.com/1371655.
  lacros_args.emplace_back(base::StringPrintf(
      "--%s=%s", switches::kGaiaUrl, https_server_->base_url().spec().c_str()));
  ash::standalone_browser::AddLacrosArguments(lacros_args, command_line);
}
