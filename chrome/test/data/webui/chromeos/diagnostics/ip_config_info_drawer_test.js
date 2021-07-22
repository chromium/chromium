// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ip_config_info_drawer.js';

import {Network} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeEthernetNetwork, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function ipConfigInfoDrawerTestSuite() {
  /** @type {?IpConfigInfoDrawerElement} */
  let ipConfigInfoDrawerElement = null;

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
        ipConfigInfoDrawerElement.$$('#ipConfigInfoElement'));
  }

  /**
   * Selects the drawer's toggle button.
   * @return {!HTMLElement}
   */
  function getDrawerToggle() {
    const toggleButton = ipConfigInfoDrawerElement.$$('cr-expand-button');
    assertTrue(!!toggleButton);
    return /** @type {!HTMLElement} */ (toggleButton);
  }

  test('IpConfigInfoDrawerInitialized', () => {
    return initializeIpConfigInfoDrawerElement().then(() => {
      dx_utils.assertElementContainsText(
          /** @type {HTMLElement} */ (
              ipConfigInfoDrawerElement.$$('cr-expand-button > span')),
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

  test('ConfigDrawerOpenDisplaysMacAddressBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement()
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#macAddress',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerMacAddress'),
              `${fakeEthernetNetwork.macAddress}`);
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
    const expectedSubnetMask = '255.255.255.0';
    return initializeIpConfigInfoDrawerElement(fakeWifiNetwork)
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#subnetMask',
              ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerSubnetMask'),
              expectedSubnetMask);
        });
  });

  test('ConfigDrawerOpenDisplaysNameServersBasedOnNetwork', () => {
    return initializeIpConfigInfoDrawerElement()
        // Opening drawer to test visibility and content of data points.
        .then(() => getDrawerToggle().click())
        .then(() => {
          dx_utils.assertDataPointHasExpectedHeaderAndValue(
              ipConfigInfoDrawerElement, '#nameServers', 'Name servers',
              `${fakeEthernetNetwork.ipConfig.nameServers.join(', ')}`);
        });
  });
}
