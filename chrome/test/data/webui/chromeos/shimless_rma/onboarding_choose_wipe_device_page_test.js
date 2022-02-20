// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnboardingChooseWipeDevicePage} from 'chrome://shimless-rma/onboarding_choose_wipe_device_page.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function onboardingChooseWipeDevicePageTest() {
  /** @type {?OnboardingChooseWipeDevicePage} */
  let component = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseWipeDevicePage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseWipeDevicePage} */ (
        document.createElement('onboarding-choose-wipe-device-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ChooseWipeDevicePageInitializes', async () => {
    await initializeChooseWipeDevicePage();

    assertTrue(!!component);
  });
}
