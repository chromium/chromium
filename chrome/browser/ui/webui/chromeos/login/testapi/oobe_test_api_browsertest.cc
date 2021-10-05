// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/hid_controller_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class OobeTestApiTest : public OobeBaseTest {
 public:
  OobeTestApiTest() = default;
  ~OobeTestApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableOobeTestAPI);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Ensure WebUI is loaded to allow Javascript execution.
    LoginDisplayHost::default_host()->GetWizardController();
  }
};

IN_PROC_BROWSER_TEST_F(OobeTestApiTest, OobeAPI) {
  test::OobeJS().CreateWaiter("window.OobeAPI")->Wait();
  test::OobeJS()
      .CreateWaiter("OobeAPI.screens.WelcomeScreen.isVisible()")
      ->Wait();
  test::OobeJS().Evaluate("OobeAPI.screens.WelcomeScreen.clickNext()");
  test::OobeJS()
      .CreateWaiter("OobeAPI.screens.NetworkScreen.isVisible()")
      ->Wait();
  test::OobeJS().Evaluate("OobeAPI.screens.NetworkScreen.clickNext()");

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  test::OobeJS().ExpectFalse("OobeAPI.screens.EulaScreen.shouldSkip()");
  test::OobeJS().CreateWaiter("OobeAPI.screens.EulaScreen.isVisible()")->Wait();
  test::OobeJS().Evaluate("OobeAPI.screens.EulaScreen.clickNext()");
#else
  test::OobeJS().ExpectTrue("OobeAPI.screens.EulaScreen.shouldSkip()");
#endif
}

class OobeTestApiTestChromebox : public OobeTestApiTest {
 public:
  OobeTestApiTestChromebox() = default;
  ~OobeTestApiTestChromebox() override = default;

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};
  base::test::ScopedChromeOSVersionInfo version_{"DEVICETYPE=CHROMEBASE",
                                                 base::Time::Now()};
};

IN_PROC_BROWSER_TEST_F(OobeTestApiTestChromebox, HIDDetectionScreen) {
  test::OobeJS().CreateWaiter("window.OobeAPI")->Wait();
  test::OobeJS()
      .CreateWaiter("OobeAPI.screens.HIDDetectionScreen.isVisible()")
      ->Wait();
  test::OobeJS().Evaluate(
      "OobeAPI.screens.HIDDetectionScreen.emulateDevicesConnected()");
  test::OobeJS()
      .CreateWaiter("OobeAPI.screens.HIDDetectionScreen.isEnabled()")
      ->Wait();
  test::OobeJS().Evaluate("OobeAPI.screens.HIDDetectionScreen.clickNext()");
}

class NoOobeTestApiTest : public OobeBaseTest {
 public:
  NoOobeTestApiTest() = default;
  ~NoOobeTestApiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(NoOobeTestApiTest, NoOobeAPI) {
  test::OobeJS().ExpectFalse("window.OobeAPI");
}

class OobeTestApiRemoraRequisitionTest : public OobeTestApiTest,
                                         public LocalStateMixin::Delegate {
 public:
  OobeTestApiRemoraRequisitionTest() = default;
  ~OobeTestApiRemoraRequisitionTest() override = default;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        policy::EnrollmentRequisitionManager::kRemoraRequisition);
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(OobeTestApiRemoraRequisitionTest, SkipsEula) {
  test::OobeJS().ExpectTrue("OobeAPI.screens.EulaScreen.shouldSkip()");
}

class OobeTestApiLoginPinTest : public OobeTestApiTest {
 public:
  OobeTestApiLoginPinTest() { login_mixin_.AppendRegularUsers(1); }

 protected:
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(OobeTestApiLoginPinTest, Success) {
  test::OobeJS().CreateWaiter("window.OobeAPI")->Wait();
  const std::string username =
      login_mixin_.users()[0].account_id.GetUserEmail();
  test::OobeJS().ExecuteAsync(base::StringPrintf(
      "OobeAPI.loginWithPin('%s', '123456')", username.c_str()));
  login_mixin_.WaitForActiveSession();
}

}  // namespace chromeos
