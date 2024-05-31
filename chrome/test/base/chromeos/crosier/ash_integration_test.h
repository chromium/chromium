// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/chromeos/crosier/chromeos_integration_arc_mixin.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace base {
class CommandLine;
}

namespace net::test_server {
class EmbeddedTestServer;
}

// Base class for tests of ash-chrome integration with the ChromeOS platform,
// like hardware daemons, graphics, kernel, etc.
//
// Tests using this base class should be added to "chromeos_integration_tests"
// and will run on devices under test (DUTs) and virtual machines (VMs).
//
// Because this class derives from InProcessBrowserTest the source files must be
// added to a target that defines HAS_OUT_OF_PROC_TEST_RUNNER. The source files
// cannot be in a shared test support target that lacks that define.
class AshIntegrationTest : public InteractiveAshTest {
 public:
  AshIntegrationTest();
  AshIntegrationTest(const AshIntegrationTest&) = delete;
  AshIntegrationTest& operator=(const AshIntegrationTest&) = delete;
  ~AshIntegrationTest() override;

  // Sets up the command line and environment variables to support Lacros (by
  // enabling the Wayland server in ash). Call this from SetUpCommandLine() if
  // your test starts Lacros.
  void SetUpCommandLineForLacros(base::CommandLine* command_line);

  // Sets up the Lacros browser manager. Call this from
  // SetUpInProcessBrowserTestFixture() if your test starts Lacros.
  void SetUpLacrosBrowserManager();

  // Waits for Ash to be ready for Lacros, including starting the "Exo" Wayland
  // server. Call this method if your test starts Lacros, otherwise Exo may not
  // be ready and Lacros may not start.
  void WaitForAshFullyStarted();

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  ChromeOSIntegrationLoginMixin& login_mixin() { return login_mixin_; }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ChromeOSIntegrationArcMixin& arc_mixin() { return arc_mixin_; }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

 private:
  // Overrides the Gaia URL to point to a local test server that produces an
  // error, which is expected behavior in test environments.
  void OverrideGaiaUrlForLacros(base::CommandLine* command_line);

  // This test runs on linux-chromeos in interactive_ui_tests and on a DUT in
  // chromeos_integration_tests.
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};

  // Login support.
  ChromeOSIntegrationLoginMixin login_mixin_{&mixin_host_};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // ARC is only supported on the branded build.
  ChromeOSIntegrationArcMixin arc_mixin_{&mixin_host_, login_mixin_};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_
