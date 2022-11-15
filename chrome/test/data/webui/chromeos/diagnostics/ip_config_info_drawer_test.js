// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ip_config_info_drawer.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {fakeEthernetNetwork, fakeWifiNetwork, fakeWifiNetworkEmptyNameServers, fakeWifiNetworkMultipleNameServers, fakeWifiNetworkNoNameServers} from 'chrome://diagnostics/fake_data.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('ipConfigInfoDrawerTestSuite', function() {
  /** @type {?IpConfigInfoDrawerElement} */
  let ipConfigInfoDrawerElement = null;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    ipConfigInfoDrawerElement.remove();
    ipConfigInfoDrawerElement = null;
  });

  /**
   * @param {!Network=} network
   */
  function initializeIpConfigInfoDrawerElement(network = fakeEthernetNetwork) {
    ipConfigInfoDrawerElement = /** @type {!IpConfigInfoDrawerElement} */ (
        document.createElement('ip-config-info-drawer'));
    ipConfigInfoDrawerElement.network = network;
    document.body.appendChild(ipConfigInfoDrawerElement);
    assertTrue(!!ipConfigInfoDrawerElement);
    return flushTasks();
  }

  /**
   * Selects the drawer's hideable content area if the drawer is expanded.
   * @return {HTMLElement}
   */
  function getDrawerContentContainer() {
    return /** @type {!HTMLElement} */ (
        ipConfigInfoDrawerElement.shadowRoot.querySelector(
            '#ipConfigInfoElement'));
  }

  /**
   * Selects the drawer's toggle button.
   * @return {!HTMLElement}
   */
  function getDrawerToggle() {
    const toggleButton =
        ipConfigInfoDrawerElement.shadowRoot.querySelector('#drawerToggle');
    assertTrue(!!toggleButton);
    return /** @type {!HTMLElement} */ (toggleButton);
  }

  /** @return {string} */
  function getNameServersHeaderText() {
    return dx_utils.getDataPoint(ipConfigInfoDrawerElement, '#nameServers')
        .header;
  }

  /**
   * @suppress {visibility}
   * @param {number} prefix
   * @return {!Promise}
   */
  function setRoutingPrefix(prefix) {
    assertTrue(
        !!ipConfigInfoDrawerElement.network &&
        !!ipConfigInfoDrawerElement.network.ipConfig);

    ipConfigInfoDrawerElement.network.ipConfig.routingPrefix = prefix;
    ipConfigInfoDrawerElement.notifyPath('network.ipConfig.routingPrefix');

    return flushTasks();
  }

  test('IpConfigInfoDrawerInitialized', () => {
    return initializeIpConfigInfoDrawerElement().then(() => {
      assertTrue(isVisible(getDrawerToggle()));
      dx_utils.assertElementContainsText(
          /** @type {HTMLElement} */ (
              ipConfigInfoDrawerElement.shadowRoot.querySelector(
                  '#drawerTitle')),
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
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerGateway'),
              `${fakeWifiNetwork.ipConfig.gateway}`);
        });
  });

  test('ConfigDrawerOpenDisplaysSubnetMaskBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetwork)
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerSubnetMask'),
              '255.255.255.0');

          return setRoutingPrefix(0);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerSubnetMask'),
              '');

          return setRoutingPrefix(32);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerSubnetMask'),
              '255.255.255.255');

          return setRoutingPrefix(33);
        })
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerSubnetMask'),
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
              `${fakeEthernetNetwork.ipConfig.nameServers.join(', ')}`);
        });
  });

  test('CorrectHeaderShownWithNoNameServers', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetworkNoNameServers)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name Servers',
              ipConfigInfoDrawerElement.i18n('networkDnsNotConfigured'));
        });
  });

  test('CorrectHeaderShownWithEmptyNameServers', () => {
    return initializeIpConfigInfoDrawerElement(fakeWifiNetworkEmptyNameServers)
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name Servers',
              ipConfigInfoDrawerElement.i18n('networkDnsNotConfigured'));
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
