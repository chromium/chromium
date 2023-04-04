// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://shimless-rma. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ShimlessRmaApp*`
 * To run a single test suite such as 'AllInputsDisabledTest':
 * browser_tests --gtest_filter=ShimlessRmaAppAllInputsDisabledTest.All
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

this.ShimlessRmaBrowserTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kShimlessRMAOsUpdate',
        'ash::features::kShimlessRMADiagnosticPage',
      ],
    };
  }

  /** @override */
  get commandLineSwitches() {
    return [{switchName: 'launch-rma'}];
  }
};

const tests = [
  ['AllInputsDisabledTest', 'all_inputs_disabled_test.js'],
  ['HardwareErrorPageTest', 'hardware_error_page_test.js'],
  ['CriticalErrorPageTest', 'critical_error_page_test.js'],
  ['DiagnosticPageTest', 'diagnostic_page_test.js'],
  ['FakeShimlessRmaServiceTestSuite', 'fake_shimless_rma_service_test.js'],
  [
    'OnboardingChooseDestinationPageTest',
    'onboarding_choose_destination_page_test.js'
  ],
  [
    'OnboardingChooseWipeDevicePageTest',
    'onboarding_choose_wipe_device_page_test.js'
  ],
  [
    'OnboardingChooseWpDisableMethodPageTest',
    'onboarding_choose_wp_disable_method_page_test.js'
  ],
  [
    'OnboardingEnterRsuWpDisableCodePageTest',
    'onboarding_enter_rsu_wp_disable_code_page_test.js'
  ],
  ['OnboardingLandingPageTest', 'onboarding_landing_page_test.js'],
  ['OnboardingNetworkPageTest', 'onboarding_network_page_test.js'],
  [
    'OnboardingSelectComponentsPageTest',
    'onboarding_select_components_page_test.js', 'DISABLED_All'
  ],
  ['OnboardingUpdatePageTest', 'onboarding_update_page_test.js'],
  [
    'OnboardingWaitForManualWpDisablePageTest',
    'onboarding_wait_for_manual_wp_disable_page_test.js'
  ],
  [
    'OnboardingWpDisableCompletePageTest',
    'onboarding_wp_disable_complete_page_test.js'
  ],
  ['RebootPageTest', 'reboot_page_test.js'],
  [
    'ReimagingCalibrationFailedPageTest',
    'reimaging_calibration_failed_page_test.js'
  ],
  ['ReimagingCalibrationRunPageTest', 'reimaging_calibration_run_page_test.js'],
  [
    'ReimagingCalibrationSetupPageTest',
    'reimaging_calibration_setup_page_test.js'
  ],
  ['ReimagingFirmwareUpdatePageTest', 'reimaging_firmware_update_page_test.js'],
  [
    'ReimagingDeviceInformationPageTest',
    'reimaging_device_information_page_test.js'
  ],
  ['ReimagingProvisioningPageTest', 'reimaging_provisioning_page_test.js'],
  ['RepairComponentChipTest', 'repair_component_chip_test.js'],
  ['ShimlessRMAAppTest', 'shimless_rma_app_test.js'],
  ['WrapupFinalizePageTest', 'wrapup_finalize_page_test.js'],
  ['WrapupRepairCompletePageTest', 'wrapup_repair_complete_page_test.js'],
  ['WrapupRestockPageTest', 'wrapup_restock_page_test.js'],
  [
    'WrapupWaitForManualWpEnablePageTest',
    'wrapup_wait_for_manual_wp_enable_page_test.js'
  ],
];

tests.forEach(test => registerTest(...test));

/*
 * Add a `caseName` to a specific test to disable it i.e. 'DISABLED_All'
 * @param {string} testName
 * @param {string} module
 * @param {string} caseName
 */
function registerTest(testName, module, caseName) {
  const className = `ShimlessRmaApp${testName}`;
  this[className] = class extends ShimlessRmaBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://shimless-rma/test_loader.html` +
          `?module=chromeos/shimless_rma/${module}&host=test`;
    }
  };
  TEST_F(className, caseName || 'All', () => mocha.run());
}
