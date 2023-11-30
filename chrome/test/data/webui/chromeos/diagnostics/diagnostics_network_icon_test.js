// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectionStateType, CrosNetworkType, DiagnosticsNetworkIconElement, networkToNetworkStateAdapter} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {fakeCellularDisabledNetwork, fakeCellularNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeEthernetNetwork, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled} from 'chrome://diagnostics/fake_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import {assertTextContains} from './diagnostics_test_utils.js';

suite('diagnosticsNetworkIconTestSuite', function() {
  /** @type {?DiagnosticsNetworkIconElement} */
  let diagnosticsNetworkIconElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    if (diagnosticsNetworkIconElement) {
      diagnosticsNetworkIconElement.remove();
    }
    diagnosticsNetworkIconElement = null;
  });

  /** @return {!PaperSpinnerLiteElement} */
  function getConnectingIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return diagnosticsNetworkIconElement.shadowRoot.querySelector(
        '#connectingIcon');
  }

  /** @return {!NetworkIconElement} */
  function getNetworkIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return diagnosticsNetworkIconElement.shadowRoot.querySelector(
        '#networkIcon');
  }

  /** @return {IronIconElement} */
  function getNetworkTechnologyIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return getNetworkIcon().shadowRoot.querySelector('#technology');
  }

  /** @return {HTMLElement} */
  function getPrimaryIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return getNetworkIcon().shadowRoot.querySelector('#icon');
  }

  /** @return {HTMLElement} */
  function getRoamingIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return getNetworkIcon().shadowRoot.querySelector('#roaming');
  }

  /** @return {HTMLElement} */
  function getSecureIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return getNetworkIcon().shadowRoot.querySelector('#secure');
  }

  /**
   * @param {!Network} network
   * @return {!Promise}
   */
  function initializeDiagnosticsNetworkIcon(network) {
    assertFalse(!!diagnosticsNetworkIconElement);

    diagnosticsNetworkIconElement =
        document.createElement('diagnostics-network-icon');
    diagnosticsNetworkIconElement.network = network;
    assertTrue(!!diagnosticsNetworkIconElement);
    document.body.appendChild(diagnosticsNetworkIconElement);

    return flushTasks();
  }

  test('DiagnosticsNetworkIcon', () => {
    return initializeDiagnosticsNetworkIcon(fakeEthernetNetwork).then(() => {
      assertTrue(getPrimaryIcon().classList.contains('ethernet'));
      assertTrue(isVisible(getPrimaryIcon()));
      assertFalse(isVisible(getNetworkTechnologyIcon()));
      assertFalse(isVisible(getConnectingIcon()));
    });
  });

  test('NetworkToNetworkStateAdapter_NetworkType', () => {
    assertEquals(
        CrosNetworkType.kEthernet,
        networkToNetworkStateAdapter(fakeEthernetNetwork).type);
    assertEquals(
        CrosNetworkType.kWiFi,
        networkToNetworkStateAdapter(fakeWifiNetwork).type);
    assertEquals(
        CrosNetworkType.kCellular,
        networkToNetworkStateAdapter(fakeCellularNetwork).type);
  });

  test('NetworkToNetworkStateAdapterTypes_ConnectionState', () => {
    assertEquals(
        ConnectionStateType.kOnline,
        networkToNetworkStateAdapter(fakeEthernetNetwork).connectionState);
    assertEquals(
        ConnectionStateType.kConnected,
        networkToNetworkStateAdapter(fakeCellularNetwork).connectionState);
    assertEquals(
        ConnectionStateType.kPortal,
        networkToNetworkStateAdapter(fakePortalWifiNetwork).connectionState);
    assertEquals(
        ConnectionStateType.kConnecting,
        networkToNetworkStateAdapter(fakeConnectingEthernetNetwork)
            .connectionState);
    // NotConnected states: Disabled, NotConnected.
    assertEquals(
        ConnectionStateType.kNotConnected,
        networkToNetworkStateAdapter(fakeWifiNetworkDisabled).connectionState);
    assertEquals(
        ConnectionStateType.kNotConnected,
        networkToNetworkStateAdapter(fakeDisconnectedEthernetNetwork)
            .connectionState);
  });

  test('NetworkToNetworkStateAdapter_Guid', () => {
    assertEquals(
        fakeEthernetNetwork.observerGuid,
        networkToNetworkStateAdapter(fakeEthernetNetwork).guid);
    assertEquals(
        fakeWifiNetwork.observerGuid,
        networkToNetworkStateAdapter(fakeWifiNetwork).guid);
    assertEquals(
        fakeCellularNetwork.observerGuid,
        networkToNetworkStateAdapter(fakeCellularNetwork).guid);
  });

  test('NetworkToNetworkStateAdapter_CellularNetworkTypeProperties', () => {
    const networkState = networkToNetworkStateAdapter(fakeCellularNetwork);
    assertEquals(
        fakeCellularNetwork.typeProperties.cellular.networkTechnology,
        networkState.typeState.cellular.networkTechnology);
    assertEquals(
        fakeCellularNetwork.typeProperties.cellular.simLocked,
        networkState.typeState.cellular.simLocked);
    assertEquals(
        fakeCellularNetwork.typeProperties.cellular.signalStrength,
        networkState.typeState.cellular.signalStrength);
    assertEquals(
        fakeCellularNetwork.typeProperties.cellular.roaming,
        networkState.typeState.cellular.roaming);
  });

  test('DiagnosticsNetworkIconCellular', () => {
    return initializeDiagnosticsNetworkIcon(fakeCellularNetwork).then(() => {
      assertTrue(isVisible(getPrimaryIcon()));
      assertTextContains(getPrimaryIcon().className, 'cellular-locked');
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getRoamingIcon()));
      assertFalse(isVisible(getNetworkTechnologyIcon()));
      assertFalse(isVisible(getConnectingIcon()));
    });
  });

  test('DiagnosticsNetworkIconCellularDisabled', () => {
    return initializeDiagnosticsNetworkIcon(fakeCellularDisabledNetwork)
        .then(() => {
          assertTrue(isVisible(getPrimaryIcon()));
          assertTextContains(
              getPrimaryIcon().className, 'cellular-not-connected');
          assertTrue(isVisible(getNetworkIcon()));
          assertFalse(isVisible(getNetworkTechnologyIcon()));
          assertFalse(isVisible(getRoamingIcon()));
          assertFalse(isVisible(getConnectingIcon()));
        });
  });

  test('NetworkToNetworkStateAdapter_WifiNetworkTypeProperties', () => {
    const networkState = networkToNetworkStateAdapter(fakeWifiNetwork);
    assertEquals(
        fakeWifiNetwork.typeProperties.wifi.signalStrength,
        networkState.typeState.wifi.signalStrength);
  });

  test('DiagnosticsNetworkIconWifi', () => {
    return initializeDiagnosticsNetworkIcon(fakePortalWifiNetwork).then(() => {
      assertTrue(isVisible(getPrimaryIcon()));
      // Class name for wifi will reflect signal strength.
      assertTextContains(getPrimaryIcon().className, 'wifi-3');
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getSecureIcon()));
      assertFalse(isVisible(getNetworkTechnologyIcon()));
      assertFalse(isVisible(getRoamingIcon()));
      assertFalse(isVisible(getConnectingIcon()));
    });
  });

  test('DiagnosticsCustomConnectingIcon', () => {
    return initializeDiagnosticsNetworkIcon(fakeConnectingEthernetNetwork)
        .then(() => {
          assertFalse(isVisible(getNetworkIcon()));
          assertTrue(isVisible(getConnectingIcon()));
          assertTextContains(
              'Ethernet network, connecting', getConnectingIcon().alt);
        });
  });
});
