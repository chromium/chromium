// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_list.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectivityCardElement} from 'chrome://diagnostics/connectivity_card.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {NetworkListElement} from 'chrome://diagnostics/network_list.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('networkListTestSuite', function() {
  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  /** @type {?NetworkListElement} */
  let networkListElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  teardown(() => {
    networkListElement.remove();
    networkListElement = null;
    provider.reset();
  });

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
            networkListElement.shadowRoot.querySelector('connectivity-card'));
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
   * Returns all network-card elements.
   * @return {!NodeList<!Element>}
   */
  function getNetworkCardElements() {
    return networkListElement.shadowRoot.querySelectorAll('network-card');
  }

  /**
   * @param {string} guid
   * @suppress {visibility} // access private member
   * @return {!Promise}
   */
  function changeActiveGuid(guid) {
    networkListElement.activeGuid = guid;
    return flushTasks();
  }

  /** @return {!HTMLElement} */
  function getSettingsLink() {
    assertTrue(!!networkListElement);

    return /** @type {!HTMLElement} */ (
        networkListElement.shadowRoot.querySelector('#settingsLink'));
  }

  /**
   * Returns list of network guids.
   * @suppress {visibility} // access private member for test
   * @return {Array<?string>}
   */
  function getOtherNetworkGuids() {
    return networkListElement.otherNetworkGuids;
  }

  /**
   * @suppress {visibility}
   * @param {boolean} state
   * @return {!Promise}
   */
  function setIsLoggedIn_(state) {
    assertTrue(!!networkListElement);
    networkListElement.isLoggedIn = state;

    return flushTasks();
  }

  test('ActiveGuidPresent', () => {
    // The network-list element sets up a NetworkListObserver as part
    // of its initialization. Registering this observer causes it to
    // fire once.
    return initializeNetworkList(fakeNetworkGuidInfoList).then(() => {
      assertEquals(
          getConnectivityCard().activeGuid,
          fakeNetworkGuidInfoList[0].activeGuid);
    });
  });

  test('ActiveGuidUpdates', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => triggerNetworkListObserver())
        .then(() => {
          // Triggering the NetworkListObserver provides
          // the second observation: fakeNetworkGuidInfoList[1].
          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[1].activeGuid);
        });
  });

  test('NetworkGuidsPresent', () => {
    let networkGuids;
    let numDomRepeatInstances;
    let networkCardElements;
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.shadowRoot.querySelector('#networkCardList')
                  .items.length;
          networkCardElements = getNetworkCardElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkCardElements.length; i++) {
            assertEquals(networkCardElements[i].guid, networkGuids[i]);
          }
          return triggerNetworkListObserver();
        })
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.shadowRoot.querySelector('#networkCardList')
                  .items.length;
          networkCardElements = getNetworkCardElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkCardElements.length; i++) {
            assertEquals(networkCardElements[i].guid, networkGuids[i]);
          }
        });
  });

  test('NetworkCardElementsPopulated', () => {
    let networkCardElements;
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => flushTasks())
        .then(() => {
          networkCardElements = getNetworkCardElements();

          // The first network list observation provides guids for Cellular
          // and WiFi. The connectivity-card is responsbile for the Ethernet
          // guid as it's the currently active guid.
          const wifiInfoElement = dx_utils.getWifiInfoElement(
              networkCardElements[0].shadowRoot.querySelector('network-info'));
          dx_utils.assertTextContains(
              wifiInfoElement.shadowRoot.querySelector('#ssid').value,
              fakeWifiNetwork.typeProperties.wifi.ssid);

          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[0].activeGuid);

          return triggerNetworkListObserver();
        })
        .then(() => flushTasks())
        .then(() => {
          networkCardElements = getNetworkCardElements();
          const cellularInfoElement = dx_utils.getCellularInfoElement(
              networkCardElements[0].shadowRoot.querySelector('network-info'));
          dx_utils.assertTextContains(
              cellularInfoElement.shadowRoot.querySelector('#iccid').value,
              fakeCellularNetwork.typeProperties.cellular.iccid);
          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[1].activeGuid);
        });
  });

  test('ConnectivityCardHiddenWithNoActiveGuid', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => changeActiveGuid(''))
        .then(
            () => assertFalse(!!networkListElement.shadowRoot.querySelector(
                'connectivity-card')));
  });

  test('SettingsLinkHiddenWhenNotLoggedIn', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => {
          assertTrue(isVisible(getSettingsLink()));

          return setIsLoggedIn_(false);
        })
        .then(() => {
          assertFalse(isVisible(getSettingsLink()));
        });
  });

  test('RecordNavigationCalled', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => {
          networkListElement.onNavigationPageChanged({isActive: false});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              0, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));

          DiagnosticsBrowserProxy.setPreviousView(NavigationView.SYSTEM);
          networkListElement.onNavigationPageChanged({isActive: true});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              1, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));
          assertArrayEquals(
              [NavigationView.SYSTEM, NavigationView.CONNECTIVITY],
              /** @type {!Array<!NavigationView>} */
              (DiagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
        });
  });

  test('TastIdentifierPresent', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(
            () => assertTrue(isVisible(
                /** @type {!HTMLElement} */ (
                    networkListElement.shadowRoot.querySelector(
                        '.diagnostics-network-list-container')))));
  });
});
