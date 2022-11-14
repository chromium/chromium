// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/hid_controller_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chromeos/ash/components/hid_detection/fake_hid_detection_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

class OobeTestApiTest : public OobeBaseTest {
 public:
  OobeTestApiTest() = default;
  ~OobeTestApiTest() override = default;

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
  if (!features::IsOobeConsolidatedConsentEnabled()) {
    test::OobeJS().ExpectFalse("OobeAPI.screens.EulaScreen.shouldSkip()");
    test::OobeJS()
        .CreateWaiter("OobeAPI.screens.EulaScreen.isReadyForTesting()")
        ->Wait();
    test::OobeJS().Evaluate("OobeAPI.screens.EulaScreen.clickNext()");
    return;
  }
#endif
  test::OobeJS().ExpectTrue("OobeAPI.screens.EulaScreen.shouldSkip()");
}

class OobeTestApiTestChromebox : public OobeTestApiTest {
 public:
  OobeTestApiTestChromebox() = default;
  ~OobeTestApiTestChromebox() override = default;

  // Called after the kOobeHidDetectionRevamp field trial has set the feature
  // flag.
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    OobeBaseTest::CreatedBrowserMainParts(browser_main_parts);
    if (!features::IsOobeHidDetectionRevampEnabled())
      return;

    HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
        std::make_unique<hid_detection::FakeHidDetectionManager>());
  }

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

  test::OobeJS().ExpectFalse(
      "OobeAPI.screens.HIDDetectionScreen.touchscreenDetected()");
  test::OobeJS().ExpectFalse(
      "OobeAPI.screens.HIDDetectionScreen.mouseDetected()");
  test::OobeJS().ExpectFalse(
      "OobeAPI.screens.HIDDetectionScreen.keyboardDetected()");
  test::OobeJS().ExpectFalse(
      "OobeAPI.screens.HIDDetectionScreen.canClickNext()");

  test::OobeJS().Evaluate(
      "OobeAPI.screens.HIDDetectionScreen.emulateDevicesConnected()");

  test::OobeJS().ExpectTrue(
      "OobeAPI.screens.HIDDetectionScreen.touchscreenDetected()");
  test::OobeJS().ExpectTrue(
      "OobeAPI.screens.HIDDetectionScreen.mouseDetected()");
  test::OobeJS().ExpectTrue(
      "OobeAPI.screens.HIDDetectionScreen.keyboardDetected()");
  test::OobeJS().ExpectTrue(
      "OobeAPI.screens.HIDDetectionScreen.canClickNext()");

  test::OobeJS().Evaluate("OobeAPI.screens.HIDDetectionScreen.clickNext()");

  test::OobeJS()
      .CreateWaiter("OobeAPI.screens.WelcomeScreen.isVisible()")
      ->Wait();
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

class OobeTestApiLoginPinTest : public OobeTestApiTest,
                                public testing::WithParamInterface<bool> {
 public:
  OobeTestApiLoginPinTest() {
    login_mixin_.AppendRegularUsers(1);

    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kUseAuthFactors);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kUseAuthFactors);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, {}, nullptr, &cryptohome_mixin_};
};

IN_PROC_BROWSER_TEST_P(OobeTestApiLoginPinTest, Success) {
  test::OobeJS().CreateWaiter("window.OobeAPI")->Wait();
  const std::string username =
      login_mixin_.users()[0].account_id.GetUserEmail();
  test::OobeJS().ExecuteAsync(base::StringPrintf(
      "OobeAPI.loginWithPin('%s', '123456')", username.c_str()));
  login_mixin_.WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(All, OobeTestApiLoginPinTest, testing::Bool());

class OobeTestApiWizardControllerTest : public OobeTestApiTest {
 public:
  OobeTestApiWizardControllerTest() = default;

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(OobeTestApiWizardControllerTest, AdvanceToScreen) {
  // Make sure that OOBE is run as a "branded" build so sync screen won't be
  // skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  login_mixin_.LoginAsNewRegularUser();

  if (features::IsOobeConsolidatedConsentEnabled())
    OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  else
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();

  test::OobeJS().ExecuteAsync(
      base::StringPrintf("OobeAPI.advanceToScreen('%s')",
                         MarketingOptInScreenView::kScreenId.name));
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(OobeTestApiWizardControllerTest, SkipPostLoginScreens) {
  // Make sure that OOBE is run as a "branded" build so sync screen won't be
  // skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  login_mixin_.LoginAsNewRegularUser();

  if (features::IsOobeConsolidatedConsentEnabled())
    OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  else
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();

  test::OobeJS().ExecuteAsync("OobeAPI.skipPostLoginScreens()");
  login_mixin_.WaitForActiveSession();
}

}  // namespace ash
