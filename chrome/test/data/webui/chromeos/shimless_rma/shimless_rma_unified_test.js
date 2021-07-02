// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeShimlessRmaServiceTestSuite} from './fake_shimless_rma_service_test.js';
import {onboardingChooseDestinationPageTest} from './onboarding_choose_destination_page_test.js';
import {onboardingChooseWpDisableMethodPageTest} from './onboarding_choose_wp_disable_method_page_test.js';
import {onboardingEnterRsuWpDisableCodePageTest} from './onboarding_enter_rsu_wp_disable_code_page_test.js';
import {onboardingLandingPageTest} from './onboarding_landing_page_test.js';
import {onboardingNetworkPageTest} from './onboarding_network_page_test.js';
import {onboardingSelectComponentsPageTest} from './onboarding_select_components_page_test.js';
import {onboardingUpdatePageTest} from './onboarding_update_page_test.js';
import {onboardingWaitForManualWpDisablePageTest} from './onboarding_wait_for_manual_wp_disable_page_test.js';
import {reimagingFirmwareUpdatePageTest} from './reimaging_firmware_update_page_test.js';
import {reimagingProvisioningPageTest} from './reimaging_provisioning_page_test.js';
import {shimlessRMAAppTest} from './shimless_rma_app_test.js';
import {wrapupRepairCompletePageTest} from './wrapup_repair_complete_page_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('FakeShimlessRmaServiceTestSuite', fakeShimlessRmaServiceTestSuite);
runSuite(
    'OnboardingChooseDestinationPageTest', onboardingChooseDestinationPageTest);
runSuite(
    'OnboardingChooseWpDisableMethodPageTest',
    onboardingChooseWpDisableMethodPageTest);
runSuite(
    'OnboardingEnterRsuWpDisableCodePageTest',
    onboardingEnterRsuWpDisableCodePageTest);
runSuite('OnboardingLandingPageTest', onboardingLandingPageTest);
runSuite('OnboardingNetworkPageTest', onboardingNetworkPageTest);
runSuite(
    'OnboardingSelectComponentsPageTest', onboardingSelectComponentsPageTest);
runSuite('OnboardingUpdatePageTest', onboardingUpdatePageTest);
runSuite(
    'OnboardingWaitForManualWpDisablePageTest',
    onboardingWaitForManualWpDisablePageTest);
runSuite('ReimagingFirmwareUpdatePageTest', reimagingFirmwareUpdatePageTest);
runSuite('ReimagingProvisioningPageTest', reimagingProvisioningPageTest);
runSuite('ShimlessRMAAppTest', shimlessRMAAppTest);
runSuite('WrapupRepairCompletePageTest', wrapupRepairCompletePageTest);
