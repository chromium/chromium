// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_list.js';

import {NetworkGuidInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkListTestSuite() {
  /** @type {?NetworkListElement} */
  let networkListElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkListElement.remove();
    networkListElement = null;
    provider.reset();
  });

  /**
   * @param {!Array<!NetworkGuidInfo>} fakeNetworkGuidInfoList
   */
  function initializeNetworkList(fakeNetworkGuidInfoList) {
    assertFalse(!!networkListElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

    // Add the network list to the DOM.
    networkListElement = /** @type {!NetworkListElement} */ (
        document.createElement('network-list'));
    assertTrue(!!networkListElement);
    document.body.appendChild(networkListElement);

    return flushTasks();
  }

  /**
   * Returns the connectivity-card element.
   * @return {!ConnectivityCardElement}
   */
  function getConnectivityCard() {
    const connectivityCard =
        /** @type {!ConnectivityCardElement} */ (
            networkListElement.$$('connectivity-card'));
    assertTrue(!!connectivityCard);
    return connectivityCard;
  }

  /**
   * Causes the network list observer to fire.
   */
  function triggerNetworkListObserver() {
    provider.triggerNetworkListObserver();
    return flushTasks();
  }

  /**
   * Returns all network-info elements.
   * @return {!NodeList<!Element>}
   */
  function getNetworkInfoElements() {
    return networkListElement.shadowRoot.querySelectorAll('network-info');
  }

  /**
   * Returns list of network guids.
   * @suppress {visibility} // access private member for test
   * @return {Array<?string>}
   */
  function getOtherNetworkGuids() {
    return networkListElement.otherNetworkGuids_;
  }

  test('ActiveGuidPresent', () => {
    // The network-list element sets up a NetworkListObserver as part
    // of its initialization. Registering this observer causes it to
    // fire once.
    return initializeNetworkList(fakeNetworkGuidInfoList).then(() => {
      dx_utils.assertElementContainsText(
          getConnectivityCard().$$('#activeGuid'),
          /**  @type {string} */ (fakeNetworkGuidInfoList[0].activeGuid));
    });
  });

  test('ActiveGuidUpdates', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => triggerNetworkListObserver())
        .then(() => {
          // Triggering the NetworkListObserver provides
          // the second observation: fakeNetworkGuidInfoList[1].
          dx_utils.assertElementContainsText(
              getConnectivityCard().$$('#activeGuid'),
              /** @type {string} */ (fakeNetworkGuidInfoList[1].activeGuid));
        });
  });

  test('NetworkGuidsPresent', () => {
    let networkGuids;
    let numDomRepeatInstances;
    let networkInfoElements;
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.$$('#networkInfoList').items.length;
          networkInfoElements = getNetworkInfoElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkInfoElements.length; i++) {
            assertEquals(networkInfoElements[i].guid, networkGuids[i]);
          }
          return triggerNetworkListObserver();
        })
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.$$('#networkInfoList').items.length;
          networkInfoElements = getNetworkInfoElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkInfoElements.length; i++) {
            assertEquals(networkInfoElements[i].guid, networkGuids[i]);
          }
        });
  });

  test('NetworkInfoElementsPopulated', () => {
    let networkInfoElements;
    return initializeNetworkList(fakeNetworkGuidInfoList).then(async () => {
      networkInfoElements = getNetworkInfoElements();
      // The first network list observation provides guids for Cellular
      // and WiFi. The connectivity-card is responsbile for the Ethernet
      // guid as it's the currently active guid.
      dx_utils.assertElementContainsText(
          networkInfoElements[0].$$('#guid'), fakeWifiNetwork.guid);
      dx_utils.assertElementContainsText(
          networkInfoElements[1].$$('#guid'), fakeCellularNetwork.guid);
    });
  });
}