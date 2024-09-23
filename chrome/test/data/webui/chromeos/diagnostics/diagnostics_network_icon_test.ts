// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectionStateType, CrosNetworkType, DiagnosticsNetworkIconElement, networkToNetworkStateAdapter} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {fakeCellularDisabledNetwork, fakeCellularNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeEthernetNetwork, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled} from 'chrome://diagnostics/fake_data.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {NetworkIconElement} from 'chrome://resources/ash/common/network/network_icon.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {assertTextContains} from './diagnostics_test_utils.js';

suite('diagnosticsNetworkIconTestSuite', function() {
  let diagnosticsNetworkIconElement: DiagnosticsNetworkIconElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    diagnosticsNetworkIconElement?.remove();
    diagnosticsNetworkIconElement = null;
  });

  function querySelector<E extends Element>(
      element: Element, selector: string): E|null {
    if (!element) {
      return null;
    }
    if (!element.shadowRoot) {
      return null;
    }

    return element.shadowRoot.querySelector<E>(selector);
  }

  function getConnectingIcon(): PaperSpinnerLiteElement {
    assert(diagnosticsNetworkIconElement);
    const connectingIcon = querySelector<PaperSpinnerLiteElement>(
      diagnosticsNetworkIconElement, '#connectingIcon');
    assert(connectingIcon);
    return connectingIcon;
  }

  function getNetworkIcon(): NetworkIconElement {
    assert(diagnosticsNetworkIconElement);
    return strictQuery(
        '#networkIcon', diagnosticsNetworkIconElement.shadowRoot,
        NetworkIconElement);
  }

  function getNetworkTechnologyIcon(): IronIconElement {
    assert(diagnosticsNetworkIconElement);
    const technologyIcon = querySelector<IronIconElement>(
      getNetworkIcon(), '#technology');
    assert(technologyIcon);
    return technologyIcon;
  }

  function getPrimaryIcon(): HTMLElement {
    return strictQuery('#icon', getNetworkIcon().shadowRoot, HTMLElement);
  }

  function getRoamingIcon(): HTMLElement {
    return strictQuery('#roaming', getNetworkIcon().shadowRoot, HTMLElement);
  }

  function getSecureIcon(): HTMLElement {
    return strictQuery('#secure', getNetworkIcon().shadowRoot, HTMLElement);
  }

  function initializeDiagnosticsNetworkIcon(network: Network): Promise<void> {
    assertFalse(!!diagnosticsNetworkIconElement);

    diagnosticsNetworkIconElement =
        document.createElement('diagnostics-network-icon');
    diagnosticsNetworkIconElement.network = network;
    assert(diagnosticsNetworkIconElement);
    document.body.appendChild(diagnosticsNetworkIconElement);

    return flushTasks();
  }

  test('DiagnosticsNetworkIcon', () => {
    return initializeDiagnosticsNetworkIcon((fakeEthernetNetwork as Network))
        .then(() => {
          assertTrue(getPrimaryIcon().classList.contains('ethernet'));
          assertTrue(isVisible(getPrimaryIcon()));
          assertFalse(isVisible(getNetworkTechnologyIcon()));
          assertFalse(isVisible(getConnectingIcon()));
        });
  });

  test('NetworkToNetworkStateAdapter_NetworkType', () => {
    assertEquals(
        CrosNetworkType.kEthernet,
        networkToNetworkStateAdapter((fakeEthernetNetwork as Network)).type);
    assertEquals(
        CrosNetworkType.kWiFi,
        networkToNetworkStateAdapter((fakeWifiNetwork as Network)).type);
    assertEquals(
        CrosNetworkType.kCellular,
        networkToNetworkStateAdapter((fakeCellularNetwork as Network)).type);
  });

  test('NetworkToNetworkStateAdapterTypes_ConnectionState', () => {
    assertEquals(
        ConnectionStateType.kOnline,
        networkToNetworkStateAdapter((fakeEthernetNetwork as Network))
            .connectionState);
    assertEquals(
        ConnectionStateType.kConnected,
        networkToNetworkStateAdapter((fakeCellularNetwork as Network))
            .connectionState);
    assertEquals(
        ConnectionStateType.kPortal,
        networkToNetworkStateAdapter((fakePortalWifiNetwork as Network))
            .connectionState);
    assertEquals(
        ConnectionStateType.kConnecting,
        networkToNetworkStateAdapter((fakeConnectingEthernetNetwork as Network))
            .connectionState);
    // NotConnected states: Disabled, NotConnected.
    assertEquals(
        ConnectionStateType.kNotConnected,
        networkToNetworkStateAdapter((fakeWifiNetworkDisabled as Network))
            .connectionState);
    assertEquals(
        ConnectionStateType.kNotConnected,
        networkToNetworkStateAdapter(
            (fakeDisconnectedEthernetNetwork as Network))
            .connectionState);
  });

  test('NetworkToNetworkStateAdapter_Guid', () => {
    assertEquals(
        fakeEthernetNetwork!.observerGuid,
        networkToNetworkStateAdapter((fakeEthernetNetwork as Network)).guid);
    assertEquals(
        fakeWifiNetwork!.observerGuid,
        networkToNetworkStateAdapter((fakeWifiNetwork as Network)).guid);
    assertEquals(
        fakeCellularNetwork!.observerGuid,
        networkToNetworkStateAdapter((fakeCellularNetwork as Network)).guid);
  });

  test('NetworkToNetworkStateAdapter_CellularNetworkTypeProperties', () => {
    const networkState =
        networkToNetworkStateAdapter((fakeCellularNetwork as Network));
    assertEquals(
        fakeCellularNetwork!.typeProperties!.cellular!.networkTechnology,
        networkState!.typeState!.cellular!.networkTechnology);
    assertEquals(
        fakeCellularNetwork!.typeProperties!.cellular!.simLocked,
        networkState!.typeState!.cellular!.simLocked);
    assertEquals(
        fakeCellularNetwork!.typeProperties!.cellular!.signalStrength,
        networkState!.typeState!.cellular!.signalStrength);
    assertEquals(
        fakeCellularNetwork!.typeProperties!.cellular!.roaming,
        networkState!.typeState!.cellular!.roaming);
  });

  test('DiagnosticsNetworkIconCellular', () => {
    return initializeDiagnosticsNetworkIcon((fakeCellularNetwork as Network))
        .then(() => {
          assertTrue(isVisible(getPrimaryIcon()));
          assertTextContains(getPrimaryIcon().className, 'cellular-locked');
          assertTrue(isVisible(getNetworkIcon()));
          assertTrue(isVisible(getRoamingIcon()));
          assertFalse(isVisible(getNetworkTechnologyIcon()));
          assertFalse(isVisible(getConnectingIcon()));
        });
  });

  test('DiagnosticsNetworkIconCellularDisabled', () => {
    return initializeDiagnosticsNetworkIcon(
               (fakeCellularDisabledNetwork as Network))
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
    const networkState =
        networkToNetworkStateAdapter((fakeWifiNetwork as Network));
    assertEquals(
        fakeWifiNetwork!.typeProperties!.wifi!.signalStrength,
        networkState.typeState!.wifi!.signalStrength);
  });

  test('DiagnosticsNetworkIconWifi', () => {
    return initializeDiagnosticsNetworkIcon((fakePortalWifiNetwork as Network))
        .then(() => {
          assertTrue(isVisible(getPrimaryIcon()));
          // Class name for wifi will reflect signal strength.
          assertTextContains('wifi-3', getPrimaryIcon().className);
          assertTrue(isVisible(getNetworkIcon()));
          assertTrue(isVisible(getSecureIcon()));
          assertFalse(isVisible(getNetworkTechnologyIcon()));
          assertFalse(isVisible(getRoamingIcon()));
          assertFalse(isVisible(getConnectingIcon()));
        });
  });

  test('DiagnosticsCustomConnectingIcon', () => {
    return initializeDiagnosticsNetworkIcon(
               (fakeConnectingEthernetNetwork as Network))
        .then(() => {
          assertFalse(isVisible(getNetworkIcon()));
          assertTrue(isVisible(getConnectingIcon()));
          assertTextContains(
              'Ethernet network, connecting',
              (getConnectingIcon().alt as string));
        });
  });
});
