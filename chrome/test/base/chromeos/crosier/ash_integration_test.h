// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_

#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/chromeos/crosier/chromeos_integration_arc_mixin.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace base {
class CommandLine;
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

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  ChromeOSIntegrationLoginMixin& login_mixin() { return login_mixin_; }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ChromeOSIntegrationArcMixin& arc_mixin() { return arc_mixin_; }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

 private:
  // This test runs on linux-chromeos in interactive_ui_tests and on a DUT in
  // chromeos_integration_tests.
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};

  // Login support.
  ChromeOSIntegrationLoginMixin login_mixin_{&mixin_host_};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // ARC is only supported on the branded build.
  ChromeOSIntegrationArcMixin arc_mixin_{&mixin_host_, login_mixin_};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_ASH_INTEGRATION_TEST_H_
