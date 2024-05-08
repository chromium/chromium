// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ip_config_info_drawer.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {CellularNetwork, EthernetNetwork, WiFiNetwork} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeEthernetNetwork, fakeWifiNetwork, fakeWifiNetworkEmptyNameServers, fakeWifiNetworkMultipleNameServers, fakeWifiNetworkNoNameServers} from 'chrome://diagnostics/fake_data.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {CrExpandButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('ipConfigInfoDrawerTestSuite', function() {
  let ipConfigInfoDrawerElement: IpConfigInfoDrawerElement|null = null;

  const DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();

  suiteSetup(() => {
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    ipConfigInfoDrawerElement?.remove();
    ipConfigInfoDrawerElement = null;
  });

  function initializeIpConfigInfoDrawerElement(
      network: WiFiNetwork|EthernetNetwork|CellularNetwork =
          fakeEthernetNetwork) {
    ipConfigInfoDrawerElement = document.createElement('ip-config-info-drawer');
    ipConfigInfoDrawerElement.network = network as Network;
    document.body.appendChild(ipConfigInfoDrawerElement);
    assert(ipConfigInfoDrawerElement);
    return flushTasks();
  }

  /**
   * Selects the drawer's hideable content area if the drawer is expanded.
   */
  function getDrawerContentContainer(): HTMLDivElement|null {
    assert(ipConfigInfoDrawerElement);
    return ipConfigInfoDrawerElement.shadowRoot!.querySelector<HTMLDivElement>(
        '#ipConfigInfoElement');
  }

  /**
   * Selects the drawer's toggle button.
   */
  function getDrawerToggle(): CrExpandButtonElement {
    assert(ipConfigInfoDrawerElement);
    const toggleButton =
        ipConfigInfoDrawerElement.shadowRoot!
            .querySelector<CrExpandButtonElement>('#drawerToggle');
    assert(toggleButton);
    return toggleButton;
  }

  function getNameServersHeaderText(): string {
    assert(ipConfigInfoDrawerElement);
    return dx_utils.getDataPoint(ipConfigInfoDrawerElement, '#nameServers')
        .header;
  }

  function setRoutingPrefix(prefix: number): Promise<void> {
    assert(ipConfigInfoDrawerElement);
    assert(ipConfigInfoDrawerElement.network);
    assert(ipConfigInfoDrawerElement.network.ipConfig);

    ipConfigInfoDrawerElement.network.ipConfig.routingPrefix = prefix;
    ipConfigInfoDrawerElement.notifyPath('network.ipConfig.routingPrefix');

    return flushTasks();
  }

  test('IpConfigInfoDrawerInitialized', () => {
    return initializeIpConfigInfoDrawerElement().then(() => {
      assert(ipConfigInfoDrawerElement);
      assertTrue(isVisible(getDrawerToggle()));
      dx_utils.assertElementContainsText(
          ipConfigInfoDrawerElement.shadowRoot!
              .querySelector<CrExpandButtonElement>('#drawerTitle'),
          ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerTitle'));
    });
  });

  test('IpConfigInfoDrawerContentVisibilityTogglesOnClick', () => {
    return initializeIpConfigInfoDrawerElement()
        .then(() => {
          // Initial state is unexpanded causing element to be hidden.
          assertFalse(!!getDrawerContentContainer());
        })
        // Click toggle button to expand drawer.
        .then(() => getDrawerToggle().click())
        .then(() => {
          // Confirm expanded state visibility is correctly updated.
          assertTrue(!!(getDrawerContentContainer()));
        });
  });

  test('ConfigDrawerOpenDisplaysGatewayBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetwork)
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#gateway',
              ipConfigInfoDrawerElement!.i18n('ipConfigInfoDrawerGateway'),
              `${fakeWifiNetwork!.ipConfig!.gateway}`);
        });
  });

  test('ConfigDrawerOpenDisplaysSubnetMaskBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetwork)
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement!.i18n('ipConfigInfoDrawerSubnetMask'),
              '255.255.255.0');

          return setRoutingPrefix(0);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement!.i18n('ipConfigInfoDrawerSubnetMask'),
              '');

          return setRoutingPrefix(32);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement!.i18n('ipConfigInfoDrawerSubnetMask'),
              '255.255.255.255');

          return setRoutingPrefix(33);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement!.i18n('ipConfigInfoDrawerSubnetMask'),
              '');
        });
  });

  test('ConfigDrawerOpenDisplaysNameServersBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement()
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name Server',
              `${fakeEthernetNetwork!.ipConfig!.nameServers!.join(', ')}`);
        });
  });

  test('CorrectHeaderShownWithNoNameServers', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetworkNoNameServers)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name Servers',
              ipConfigInfoDrawerElement!.i18n('networkDnsNotConfigured'));
        });
  });

  test('CorrectHeaderShownWithEmptyNameServers', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetworkEmptyNameServers)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name Servers',
              ipConfigInfoDrawerElement!.i18n('networkDnsNotConfigured'));
        });
  });

  test('CorrectHeaderShownWithOneNameServer', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetwork)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertTextContains(
              getNameServersHeaderText(), 'Name Server');
        });
  });

  test('CorrectHeaderShownWithMultipleNameServers', () => {
    return initializeIpConfigInfoDrawerElement(
               fakeWifiNetworkMultipleNameServers)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertTextContains(
              getNameServersHeaderText(), 'Name Servers');
        });
  });
});
