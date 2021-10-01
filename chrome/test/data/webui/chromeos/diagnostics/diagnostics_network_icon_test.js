// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConnectionStateType, CrosNetworkType, DiagnosticsNetworkIconElement, networkToNetworkStateAdapter} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {fakeCellularNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeEthernetNetwork, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled} from 'chrome://diagnostics/fake_data.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

export function diagnosticsNetworkIconTestSuite() {
  /** @type {?DiagnosticsNetworkIconElement} */
  let diagnosticsNetworkIconElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (diagnosticsNetworkIconElement) {
      diagnosticsNetworkIconElement.remove();
    }
    diagnosticsNetworkIconElement = null;
  });

  /** @return {!NetworkIconElement} */
  function getNetworkIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return diagnosticsNetworkIconElement.shadowRoot.querySelector(
        '#networkIcon');
  }

  /** @return {!HTMLElement} */
  function getPrimaryIcon() {
    assertTrue(!!diagnosticsNetworkIconElement);

    return getNetworkIcon().shadowRoot.querySelector('#icon');
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
}
