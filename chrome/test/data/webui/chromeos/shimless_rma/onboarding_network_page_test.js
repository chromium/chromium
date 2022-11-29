// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {fakeNetworks} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setNetworkConfigServiceForTesting, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingNetworkPage} from 'chrome://shimless-rma/onboarding_network_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

suite('onboardingNetworkPageTest', function() {
  /** @type {?OnboardingNetworkPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let shimlessRmaService = null;

  /** @type {?FakeNetworkConfig} */
  let networkConfigService = null;

  setup(() => {
    document.body.innerHTML = '';
    shimlessRmaService = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(shimlessRmaService);
    networkConfigService = new FakeNetworkConfig();
    setNetworkConfigServiceForTesting(networkConfigService);
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
  function initializeOnboardingNetworkPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingNetworkPage} */ (
        document.createElement('onboarding-network-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function openNetworkConfigDialog() {
    assertTrue(!!component);

    const networkList = component.shadowRoot.querySelector('#networkList');
    const network = networkList.networks[1];
    component.showConfig_(
        network.type,
        /* empty guid since network_config.js is not mocked */ undefined,
        'eth0');

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeOnboardingNetworkPage();
    assertTrue(!!component);

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
  });


  test('PopulatesNetworkList', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
    assertEquals(networkList.networks[0].guid, 'eth0_guid');
    assertEquals(networkList.networks[1].guid, 'wifi0_guid');
  });

  test('NetworkSelectionDialog', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    component.onNetworkSelected_({detail: networkList.networks[1]});
    await flushTasks();

    const networkDialog = component.shadowRoot.querySelector('#networkConfig');
    assertTrue(!!networkDialog);
    assertFalse(networkDialog.enableConnect);

    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    assertTrue(connectButton.disabled);
  });

  test('DialogConnectButtonBindsToDialog', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    await openNetworkConfigDialog();

    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    assertTrue(connectButton.disabled);

    const networkDialog = component.shadowRoot.querySelector('#networkConfig');
    networkDialog.enableConnect = true;
    await flushTasks();

    assertFalse(connectButton.disabled);
  });

  test('DialogCloses', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    const dialog = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#dialog'));
    assertFalse(dialog.open);

    await openNetworkConfigDialog();

    const cancelButton = component.shadowRoot.querySelector('#cancelButton');
    assertFalse(cancelButton.disabled);
    assertTrue(dialog.open);

    cancelButton.click();
    await flushTasks();

    assertFalse(dialog.open);
  });

  test('DialogReopensAfterHittingConnect', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    const networkList = component.shadowRoot.querySelector('#networkList');

    // Add fake unconnected wifi.
    const fakeWiFi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    fakeWiFi.connectionState = ConnectionStateType.kNotConnected;
    networkConfigService.addNetworksForTest(fakeWiFi);
    component.refreshNetworks();
    await flushTasks();

    // Open network dialog.
    const network = networkList.networks[networkList.networks.length - 1];

    const dialog = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#dialog'));
    assertFalse(dialog.open);
    component.showConfig_(network.type, network.guid, network.name);
    assertTrue(dialog.open);
    await flushTasks();

    // Click connect button and dialog will be closed.
    component.onNetworkSelected_({detail: network});
    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    assertFalse(connectButton.hidden);
    connectButton.click();
    component.refreshNetworks();
    await flushTasks();
    assertFalse(dialog.open);

    // Reopen the same dialog.
    const dialog2 = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#dialog'));
    assertFalse(dialog2.open);
    component.showConfig_(network.type, network.guid, network.name);
    assertTrue(dialog2.open);
  });

  test('DisconnectNetwork', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    const networkList = component.shadowRoot.querySelector('#networkList');

    // Add fake connected wifi.
    const fakeWiFi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    fakeWiFi.connectionState = ConnectionStateType.kConnected;
    networkConfigService.addNetworksForTest(fakeWiFi);
    component.refreshNetworks();
    await flushTasks();

    // fake WiFi connectionState should be 'Connected'.
    const length = networkList.networks.length;
    const network = networkList.networks[length - 1];
    assertEquals(network.connectionState, ConnectionStateType.kConnected);

    // Show the 'disconnect' button instead of 'connect'.
    component.onNetworkSelected_({detail: network});
    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    const disconnectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#disconnectButton'));
    assertTrue(connectButton.hidden);
    assertFalse(disconnectButton.hidden);

    disconnectButton.click();
    component.refreshNetworks();
    await flushTasks();
    assertEquals(network.connectionState, ConnectionStateType.kNotConnected);
  });

  test('SetSkipButtonWhenNotConnected', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);

    component = /** @type {!OnboardingNetworkPage} */ (
        document.createElement('onboarding-network-page'));
    let buttonLabelKey;
    component.addEventListener('set-next-button-label', (e) => {
      buttonLabelKey = e.detail;
    });

    document.body.appendChild(component);
    await flushTasks();
    assertEquals('skipButtonLabel', buttonLabelKey);

    const ethernetConnected =
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'ethernet');
    ethernetConnected.connectionState = ConnectionStateType.kOnline;
    networkConfigService.addNetworksForTest([ethernetConnected]);

    component.refreshNetworks();
    await flushTasks();
    assertEquals('nextButtonLabel', buttonLabelKey);
  });

  test('DisableNetworkList', async () => {
    await initializeOnboardingNetworkPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertEquals(undefined, networkList.disabled);
    component.allButtonsDisabled = true;
    assertTrue(networkList.disabled);
  });

  test('TrackConfiguredNetworksCalled', async () => {
    await initializeOnboardingNetworkPage();

    // trackConfiguredNetworks() should be called during
    // 'onboarding-network-page' initialization.
    assertTrue(shimlessRmaService.getTrackConfiguredNetworks());
  });
});
