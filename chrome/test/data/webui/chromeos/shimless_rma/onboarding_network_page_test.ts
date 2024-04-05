// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import {NetworkListElement} from 'chrome://resources/ash/common/network/network_list.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {SET_NEXT_BUTTON_LABEL} from 'chrome://shimless-rma/events.js';
import {fakeNetworks} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setNetworkConfigServiceForTesting, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingNetworkPage} from 'chrome://shimless-rma/onboarding_network_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('onboardingNetworkPageTest', function() {
  let component: OnboardingNetworkPage|null = null;

  let shimlessRmaService: FakeShimlessRmaService|null = null;

  let networkConfigService: FakeNetworkConfig|null = null;

  const networkListSelector = '#networkList';
  const networkConfigSelector = '#networkConfig';
  const connectButtonSelector = '#connectButton';
  const networkDialogSelector = '#dialog';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    shimlessRmaService = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(shimlessRmaService);
    networkConfigService = new FakeNetworkConfig();
    setNetworkConfigServiceForTesting(networkConfigService);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaService = null;
    networkConfigService = null;
  });

  function initializeOnboardingNetworkPage(): Promise<void> {
    assert(!component);
    component = document.createElement(OnboardingNetworkPage.is);
    assert(component);
    document.body.appendChild(component);
    return flushTasks();
  }

  function openNetworkConfigDialog(): Promise<void> {
    assert(component);
    // TODO (b/333120446): Use NetworkListElement directly instead of
    // HTMLElement once available.
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;
    const network = networkList.networks[1] as NetworkStateProperties;
    component.showConfigForTesting(
        network.type,
        /* empty guid since network_config.js is not mocked */ '',
        /* name= */ 'eth0');
    return flushTasks();
  }

  // Verify the component renders.
  test('ComponentRenders', async () => {
    await initializeOnboardingNetworkPage();
    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;
    assert(networkList);
  });

  // Verify the network list populates with networks.
  test('PopulatesNetworkList', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;
    assert(networkList);
    assertEquals('eth0_guid', networkList.networks[0]!.guid);
    assertEquals('wifi0_guid', networkList.networks[1]!.guid);
  });

  // Verify after a successful network selection the connect sections are
  // disabled.
  test('NetworkSelectionDialog', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;
    networkList.dispatchEvent(
        new CustomEvent('selected', {detail: networkList.networks[1]}));
    await flushTasks();

    const networkConfig =
        strictQuery(networkConfigSelector, component.shadowRoot, HTMLElement) as
        NetworkConfigElement;
    assert(networkConfig);
    assertFalse(networkConfig.enableConnect);
    assertTrue(strictQuery(
                   connectButtonSelector, component.shadowRoot, CrButtonElement)
                   .disabled);
  });

  // Verify the connect button starts disabled until the network config enables
  // it.
  test('DialogConnectButtonBindsToDialog', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    await openNetworkConfigDialog();

    assert(component);
    const connectButton = strictQuery(
        connectButtonSelector, component.shadowRoot, CrButtonElement);
    assertTrue(connectButton.disabled);

    const networkConfig =
        strictQuery(networkConfigSelector, component.shadowRoot, HTMLElement) as
        NetworkConfigElement;
    networkConfig.enableConnect = true;
    await flushTasks();

    assertFalse(connectButton.disabled);
  });

  // Verify the network dialog can be closed.
  test('DialogCloses', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();

    assert(component);
    const dialog = strictQuery(
        networkDialogSelector, component.shadowRoot, CrDialogElement);
    assertFalse(dialog.open);
    await openNetworkConfigDialog();

    assertTrue(strictQuery(
                   networkDialogSelector, component.shadowRoot, CrDialogElement)
                   .open);
    const cancelButton =
        strictQuery('#cancelButton', component.shadowRoot, CrButtonElement);
    assertFalse(cancelButton.disabled);

    cancelButton.click();
    await flushTasks();

    assertFalse(dialog.open);
  });

  // Verify the network dialog can be re-opened after connecting to a network.
  test('DialogReopensAfterHittingConnect', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;

    // Add fake unconnected wifi.
    const fakeWiFi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    fakeWiFi.connectionState = ConnectionStateType.kNotConnected;
    networkConfigService.addNetworksForTest([fakeWiFi]);
    component.refreshNetworks();
    await flushTasks();

    // Open network dialog.
    const network = networkList.networks[networkList.networks.length - 1] as
        NetworkStateProperties;

    const dialog = strictQuery(
        networkDialogSelector, component.shadowRoot, CrDialogElement);
    assertFalse(dialog.open);
    component.showConfigForTesting(network.type, network.guid, network.name);
    assertTrue(dialog.open);
    await flushTasks();

    // Select a network.
    networkList.dispatchEvent(new CustomEvent('selected', {detail: network}));

    // Click connect button and dialog will be closed.
    const connectButton = strictQuery(
        connectButtonSelector, component.shadowRoot, CrButtonElement);
    assertFalse(connectButton.hidden);
    connectButton.click();
    component.refreshNetworks();
    await flushTasks();
    assertFalse(dialog.open);

    // Reopen the same dialog.
    const dialog2 = strictQuery(
        networkDialogSelector, component.shadowRoot, CrDialogElement);
    assertFalse(dialog2.open);
    component.showConfigForTesting(network.type, network.guid, network.name);
    assertTrue(dialog2.open);
  });

  // Verify a connected network can be disconnected.
  test('DisconnectNetwork', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeOnboardingNetworkPage();
    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;

    // Add fake connected wifi.
    const fakeWiFi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    fakeWiFi.connectionState = ConnectionStateType.kConnected;
    networkConfigService.addNetworksForTest([fakeWiFi]);
    component.refreshNetworks();
    await flushTasks();

    // fake WiFi connectionState should be 'Connected'.
    const network = networkList.networks[networkList.networks.length - 1] as
        NetworkStateProperties;
    assert(network);
    assertEquals(network.connectionState, ConnectionStateType.kConnected);

    // Select a network.
    networkList.dispatchEvent(new CustomEvent('selected', {detail: network}));

    // Show the 'disconnect' button instead of 'connect'.
    const connectButton = strictQuery(
        connectButtonSelector, component.shadowRoot, CrButtonElement);
    const disconnectButton =
        strictQuery('#disconnectButton', component.shadowRoot, CrButtonElement);
    assertTrue(connectButton.hidden);
    assertFalse(disconnectButton.hidden);

    disconnectButton.click();
    component.refreshNetworks();
    await flushTasks();
    assertEquals(network.connectionState, ConnectionStateType.kNotConnected);
  });

  // Verify next button shows 'Skip' when not connected to a network.
  test('SetSkipButtonWhenNotConnected', async () => {
    assert(networkConfigService);
    networkConfigService.addNetworksForTest(fakeNetworks);

    // Reset the component.
    component = document.createElement(OnboardingNetworkPage.is);
    const skipButtonLabelEvent =
        eventToPromise(SET_NEXT_BUTTON_LABEL, component);
    let buttonLabelKey;
    component.addEventListener(SET_NEXT_BUTTON_LABEL, (e: CustomEvent) => {
      buttonLabelKey = e.detail;
    });

    document.body.appendChild(component);
    await flushTasks();
    await skipButtonLabelEvent;
    assertEquals('skipButtonLabel', buttonLabelKey);

    // Connect to a network and expect the next button label event.
    const nextButtonLabelEvent =
        eventToPromise(SET_NEXT_BUTTON_LABEL, component);
    const ethernetConnected =
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'ethernet');
    ethernetConnected.connectionState = ConnectionStateType.kOnline;
    networkConfigService.addNetworksForTest([ethernetConnected]);

    component.refreshNetworks();
    await flushTasks();
    await nextButtonLabelEvent;
    assertEquals('nextButtonLabel', buttonLabelKey);
  });

  // Verify the network list is disabled when `allButtonsDisabled` is set.
  test('DisableNetworkList', async () => {
    await initializeOnboardingNetworkPage();

    assert(component);
    const networkList =
        strictQuery(networkListSelector, component.shadowRoot, HTMLElement) as
        NetworkListElement;
    assertEquals(undefined, networkList.disabled);
    component.allButtonsDisabled = true;
    assertTrue(networkList.disabled);
  });

  // Verify `trackConfiguredNetworks()` is called in page initialization;
  test('TrackConfiguredNetworksCalled', async () => {
    await initializeOnboardingNetworkPage();

    assert(shimlessRmaService);
    assertTrue(shimlessRmaService.getTrackConfiguredNetworks());
  });
});
