// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingLandingPageTest() {
  /** @type {?OnboardingLandingPage} */
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
  function initializeLandingPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingLandingPage} */ (
        document.createElement('onboarding-landing-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeLandingPage();
    assertTrue(!!component);

    const basePage = component.shadowRoot.querySelector('base-page');
    assertTrue(!!basePage);
  });
}
