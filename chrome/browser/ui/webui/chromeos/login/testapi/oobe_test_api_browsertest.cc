// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/hid_controller_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class OobeTestApiTest : public OobeBaseTest {
 public:
  OobeTestApiTest() {}
  ~OobeTestApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableOobeTestAPI);
    OobeBaseTest::SetUpCommandLine(command_line);
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
  OobeTestApiTestChromebox() {
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBASE",
                                                 base::Time::Now());
  }
  ~OobeTestApiTestChromebox() override {}

 protected:
  test::HIDControllerMixin hid_controller_{&mixin_host_};
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
  NoOobeTestApiTest() {}
  ~NoOobeTestApiTest() override {}
};

IN_PROC_BROWSER_TEST_F(NoOobeTestApiTest, NoOobeAPI) {
  test::OobeJS().ExpectFalse("window.OobeAPI");
}

}  // namespace chromeos
