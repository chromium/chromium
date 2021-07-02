// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {setNetworkConfigServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {RmaState} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';
import {FakeNetworkConfig} from '../fake_network_config_mojom.m.js';


export function onboardingLandingPageTest() {
  /** @type {?OnboardingLandingPage} */
  let component = null;

  /** @type {?FakeNetworkConfig} */
  let networkConfigService = null;

  suiteSetup(() => {
    networkConfigService = new FakeNetworkConfig();
    setNetworkConfigServiceForTesting(
        /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigInterface} */
        (networkConfigService));
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    networkConfigService.resetForTest();
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
    networkConfigService.setNetworkConnectionStateForTest(
        'eth0_guid', chromeos.networkConfig.mojom.ConnectionStateType.kOnline);
    await initializeLandingPage();

    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    await flushTasks();

    assertEquals(savedResult.state, RmaState.kUpdateChrome);
  });

  test('NoNetworkNext', async () => {
    await initializeLandingPage();

    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    await flushTasks();

    assertEquals(savedResult, undefined);
  });
}
