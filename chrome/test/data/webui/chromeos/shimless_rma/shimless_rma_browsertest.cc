// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://shimless-rma. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ShimlessRma*`
 * To run a single test suite such as 'AllInputsDisabled':
 * browser_tests --gtest_filter=ShimlessRmaBrowserTest.AllInputsDisabled
 */

namespace ash {
namespace {

class ShimlessRmaBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ShimlessRmaBrowserTest() {
    set_test_loader_host(::ash::kChromeUIShimlessRMAHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLaunchRma);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kShimlessRMAOsUpdate};
};

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, AllInputsDisabled) {
  RunTest("chromeos/shimless_rma/all_inputs_disabled_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, HardwareErrorPage) {
  RunTest("chromeos/shimless_rma/hardware_error_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, CriticalErrorPage) {
  RunTest("chromeos/shimless_rma/critical_error_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, FakeShimlessRmaService) {
  RunTest("chromeos/shimless_rma/fake_shimless_rma_service_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest,
                       OnboardingChooseDestinationPage) {
  RunTest("chromeos/shimless_rma/onboarding_choose_destination_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, OnboardingChooseWipeDevicePage) {
  RunTest("chromeos/shimless_rma/onboarding_choose_wipe_device_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest,
                       OnboardingChooseWpDisableMethodPage) {
  RunTest(
      "chromeos/shimless_rma/onboarding_choose_wp_disable_method_page_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest,
                       OnboardingEnterRsuWpDisableCodePage) {
  RunTest(
      "chromeos/shimless_rma/onboarding_enter_rsu_wp_disable_code_page_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, OnboardingLandingPage) {
  RunTest("chromeos/shimless_rma/onboarding_landing_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, OnboardingNetworkPage) {
  RunTest("chromeos/shimless_rma/onboarding_network_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, SelectComponentsPage) {
  RunTest("chromeos/shimless_rma/onboarding_select_components_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, UpdatePage) {
  RunTest("chromeos/shimless_rma/onboarding_update_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, WaitForManualWpDisable) {
  RunTest(
      "chromeos/shimless_rma/"
      "onboarding_wait_for_manual_wp_disable_page_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, WpDisableCompletePage) {
  RunTest("chromeos/shimless_rma/onboarding_wp_disable_complete_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, RebootPage) {
  RunTest("chromeos/shimless_rma/reboot_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, CalibrationFailedPage) {
  RunTest("chromeos/shimless_rma/reimaging_calibration_failed_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, CalibrationRunPage) {
  RunTest("chromeos/shimless_rma/reimaging_calibration_run_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, CalibrationSetupPage) {
  RunTest("chromeos/shimless_rma/reimaging_calibration_setup_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, FirmwareUpdatePage) {
  RunTest("chromeos/shimless_rma/reimaging_firmware_update_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, RestockPage) {
  RunTest("chromeos/shimless_rma/wrapup_restock_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, DeviceInformationPage) {
  RunTest("chromeos/shimless_rma/reimaging_device_information_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, ShimlessRmaAppPage) {
  RunTest("chromeos/shimless_rma/shimless_rma_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, ProvisioningPage) {
  RunTest("chromeos/shimless_rma/reimaging_provisioning_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, RepairComponentChip) {
  RunTest("chromeos/shimless_rma/repair_component_chip_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, FinalizePage) {
  RunTest("chromeos/shimless_rma/wrapup_finalize_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, RepairCompletePage) {
  RunTest("chromeos/shimless_rma/wrapup_repair_complete_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, WaitForManualWpEnable) {
  RunTest("chromeos/shimless_rma/wrapup_wait_for_manual_wp_enable_page_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ShimlessRmaBrowserTest, Shimless3pDiag) {
  RunTest("chromeos/shimless_rma/shimless_3p_diag_test.js", "mocha.run()");
}

}  // namespace

}  // namespace ash
