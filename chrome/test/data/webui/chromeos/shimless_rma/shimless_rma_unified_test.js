// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeShimlessRmaServiceTestSuite} from './fake_shimless_rma_service_test.js';
import {onboardingChooseWpDisableMethodPageTest} from './onboarding_choose_wp_disable_method_page_test.js';
import {onboardingEnterRsuWpDisableCodePageTest} from './onboarding_enter_rsu_wp_disable_code_page_test.js';
import {onboardingUpdatePageTest} from './onboarding_update_page_test.js';
import {onboardingWaitForManualWpDisablePageTest} from './onboarding_wait_for_manual_wp_disable_page_test.js';
import {shimlessRMAAppTest} from './shimless_rma_app_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('FakeShimlessRmaServiceTestSuite', fakeShimlessRmaServiceTestSuite);
runSuite('ShimlessRMAAppTest', shimlessRMAAppTest);
runSuite(
    'OnboardingChooseWpDisableMethodPageTest',
    onboardingChooseWpDisableMethodPageTest);
runSuite(
    'OnboardingEnterRsuWpDisableCodePageTest',
    onboardingEnterRsuWpDisableCodePageTest);
runSuite('OnboardingUpdatePageTest', onboardingUpdatePageTest);
runSuite(
    'OnboardingWaitForManualWpDisablePageTest',
    onboardingWaitForManualWpDisablePageTest);
