// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeNetworks} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setNetworkConfigServiceForTesting, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingNetworkPage} from 'chrome://shimless-rma/onboarding_network_page.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingNetworkPageTest() {
  /** @type {?OnboardingNetworkPageElement} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let shimlessRmaService = null;

  /** @type {?FakeNetworkConfig} */
  let networkConfigService = null;

  suiteSetup(() => {
    shimlessRmaService = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(shimlessRmaService);
    networkConfigService = new FakeNetworkConfig();
    setNetworkConfigServiceForTesting(networkConfigService);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    shimlessRmaService.reset();
    networkConfigService.resetForTest();
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseDestinationPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingNetworkPageElement} */ (
        document.createElement('onboarding-network-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeChooseDestinationPage();
    assertTrue(!!component);

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
  });


  test('PopulatesNetworkList', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeChooseDestinationPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
    assertEquals(networkList.networks[0].guid, 'eth0_guid');
    assertEquals(networkList.networks[1].guid, 'wifi0_guid');
  });
}
