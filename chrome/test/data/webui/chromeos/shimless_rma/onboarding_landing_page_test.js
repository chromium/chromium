// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {RmaState} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';


export function onboardingLandingPageTest() {
  /** @type {?OnboardingLandingPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  suiteSetup(() => {
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
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

  test('ConnectedNetworkNext', async () => {
    service.setCheckForNetworkConnection(fakeStates[2]);
    await initializeLandingPage();

    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    await flushTasks();

    assertEquals(savedResult.state, RmaState.kUpdateChrome);
  });

  test('NoNetworkNext', async () => {
    service.setCheckForNetworkConnection(fakeStates[1]);
    await initializeLandingPage();

    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    await flushTasks();

    assertEquals(savedResult.state, RmaState.kConfigureNetwork);
  });
}
