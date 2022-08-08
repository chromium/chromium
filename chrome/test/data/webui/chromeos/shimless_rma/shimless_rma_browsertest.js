// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for chrome://shimless_rma.
 * Unifieid polymer testing suite for shimless rma flow.
 *
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=ShimlessRMABrowserTest*``
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual --gtest_filter=ShimlessRMABrowserTest.MANUAL_*``
 *
 * To run a single test suite, such as 'AppTest':
 * `browser_tests --run-manual
 * --gtest_filter=ShimlessRMABrowserTest.MANUAL_AppTest`
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

this.ShimlessRMABrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://shimless-rma/test_loader.html?module=chromeos/' +
        'shimless_rma/shimless_rma_unified_test.js&host=test';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kShimlessRMAFlow',
        'chromeos::features::kShimlessRMAEnableStandalone',
        'chromeos::features::kShimlessRMAOsUpdate',
      ],
    };
  }
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'AllInputsDisabledTest',
  'CriticalErrorPageTest',
  'FakeShimlessRmaServiceTestSuite',
  'HardwareErrorPageTest',
  'OnboardingChooseDestinationPageTest',
  'OnboardingChooseWipeDevicePageTest',
  'OnboardingChooseWpDisableMethodPageTest',
  'OnboardingEnterRsuWpDisableCodePageTest',
  'OnboardingLandingPageTest',
  'OnboardingNetworkPageTest',
  'OnboardingSelectComponentsPageTest',
  'OnboardingUpdatePageTest',
  'OnboardingWaitForManualWpDisablePageTest',
  'OnboardingWpDisableCompletePageTest',
  'RebootPageTest',
  'ReimagingCalibrationFailedPageTest',
  'ReimagingCalibrationRunPageTest',
  'ReimagingCalibrationSetupPageTest',
  'ReimagingFirmwareUpdatePageTest',
  'ReimagingDeviceInformationPageTest',
  'ReimagingProvisioningPageTest',
  'RepairComponentChipTest',
  'ShimlessRMAAppTest',
  'WrapupFinalizePageTest',
  'WrapupRepairCompletePageTest',
  'WrapupRestockPageTest',
  'WrapupWaitForManualWpEnablePageTest',
];

TEST_F('ShimlessRMABrowserTest', 'All', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('ShimlessRMABrowserTest', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
