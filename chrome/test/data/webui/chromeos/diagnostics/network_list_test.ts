// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_list.js';

import {ConnectivityCardElement} from 'chrome://diagnostics/connectivity_card.js';
import {DataPointElement} from 'chrome://diagnostics/data_point.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView, NetworkGuidInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {NetworkCardElement} from 'chrome://diagnostics/network_card.js';
import {NetworkInfoElement} from 'chrome://diagnostics/network_info.js';
import {NetworkListElement} from 'chrome://diagnostics/network_list.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('networkListTestSuite', function() {
  const DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();

  let networkListElement: NetworkListElement|null = null;

  const provider = new FakeNetworkHealthProvider();

  const routineController = new FakeSystemRoutineController();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suiteSetup(() => {
    setNetworkHealthProviderForTesting(provider);

    // Setup a fake routine controller.
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);

    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);
  });

  teardown(() => {
    networkListElement?.remove();
    networkListElement = null;
    provider.reset();
  });

  function initializeNetworkList(fakeNetworkGuidInfoList: NetworkGuidInfo[]):
      Promise<void> {
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

    // Add the network list to the DOM.
    networkListElement = document.createElement(NetworkListElement.is);
    assert(networkListElement);
    document.body.appendChild(networkListElement);

    return flushTasks();
  }

  /**
   * Returns the connectivity-card element.
   */
  function getConnectivityCard(): ConnectivityCardElement {
    assert(networkListElement);
    return strictQuery(
        ConnectivityCardElement.is, networkListElement.shadowRoot,
        ConnectivityCardElement);
  }

  /**
   * Causes the network list observer to fire.
   */
  function triggerNetworkListObserver(): Promise<void> {
    provider.triggerNetworkListObserver();
    return flushTasks();
  }

  function setIsLoggedIn(isLoggedIn: boolean): Promise<void> {
    assert(networkListElement);
    networkListElement.setIsLoggedInForTesting(isLoggedIn);
    return flushTasks();
  }

  /**
   * Returns all network-card elements.
   */
  function getNetworkCardElements(): NodeListOf<NetworkCardElement> {
    assert(networkListElement);
    return networkListElement.shadowRoot!.querySelectorAll(
        NetworkCardElement.is);
  }

  function changeActiveGuid(guid: string): Promise<void> {
    assert(networkListElement);
    networkListElement.setActiveGuidForTesting(guid);
    return flushTasks();
  }

  function getSettingsLink() {
    assert(networkListElement);
    return strictQuery(
        '#settingsLink', networkListElement.shadowRoot, HTMLDivElement);
  }

  /**
   * Returns list of network guids.
   */
  function getOtherNetworkGuids(): string[] {
    assert(networkListElement);
    return networkListElement.getOtherNetworkGuidsForTesting();
  }

  test('ActiveGuidPresent', async () => {
    // The network-list element sets up a NetworkListObserver as part
    // of its initialization. Registering this observer causes it to
    // fire once.
    await initializeNetworkList(fakeNetworkGuidInfoList);
    assertEquals(
        getConnectivityCard().activeGuid,
        fakeNetworkGuidInfoList[0]!.activeGuid);
  });

  test('ActiveGuidUpdates', async () => {
    await initializeNetworkList(fakeNetworkGuidInfoList);
    await triggerNetworkListObserver();
    // Triggering the NetworkListObserver provides
    // the second observation: fakeNetworkGuidInfoList[1].
    assertEquals(
        getConnectivityCard().activeGuid,
        fakeNetworkGuidInfoList[1]!.activeGuid);
  });

  test('NetworkGuidsPresent', async () => {
    let networkGuids;
    let numDomRepeatInstances;
    let networkCardElements;
    await initializeNetworkList(fakeNetworkGuidInfoList);
    networkGuids = getOtherNetworkGuids();
    assert(networkListElement);
    numDomRepeatInstances =
        networkListElement.shadowRoot!
            .querySelector<DomRepeat>('#networkCardList')!.items!.length;
    networkCardElements = getNetworkCardElements();
    assertEquals(numDomRepeatInstances, networkGuids.length);
    for (let i = 0; i < networkCardElements.length; i++) {
      assertEquals(networkCardElements[i]!.guid, networkGuids[i]);
    }
    await triggerNetworkListObserver();
    networkGuids = getOtherNetworkGuids();
    numDomRepeatInstances =
        networkListElement.shadowRoot!
            .querySelector<DomRepeat>('#networkCardList')!.items!.length;
    networkCardElements = getNetworkCardElements();
    assertEquals(numDomRepeatInstances, networkGuids.length);
    for (let i = 0; i < networkCardElements.length; i++) {
      assertEquals(networkCardElements[i]!.guid, networkGuids[i]);
    }
  });

  test('NetworkCardElementsPopulated', async () => {
    let networkCardElements;
    await initializeNetworkList(fakeNetworkGuidInfoList);
    await flushTasks();
    networkCardElements = getNetworkCardElements();
    // The first network list observation provides guids for Cellular
    // and WiFi. The connectivity-card is responsible for the Ethernet
    // guid as it's the currently active guid.
    const wifiInfoElement = dx_utils.getWifiInfoElement(
        networkCardElements[0]!.shadowRoot!.querySelector(
            NetworkInfoElement.is));
    assert(wifiInfoElement);
    dx_utils.assertTextContains(
        wifiInfoElement.shadowRoot!.querySelector<DataPointElement>(
                                       '#ssid')!.value,
        fakeWifiNetwork!.typeProperties!.wifi!.ssid);
    assertEquals(
        getConnectivityCard().activeGuid,
        fakeNetworkGuidInfoList[0]!.activeGuid);
    await triggerNetworkListObserver();
    await flushTasks();
    networkCardElements = getNetworkCardElements();
    const cellularInfoElement = dx_utils.getCellularInfoElement(
        networkCardElements[0]!.shadowRoot!.querySelector(
            NetworkInfoElement.is));
    assert(cellularInfoElement);
    dx_utils.assertTextContains(
        cellularInfoElement.shadowRoot!
            .querySelector<DataPointElement>('#iccid')!.value,
        fakeCellularNetwork!.typeProperties!.cellular!.iccid);
    assertEquals(
        getConnectivityCard().activeGuid,
        fakeNetworkGuidInfoList[1]!.activeGuid);
  });

  test('ConnectivityCardHiddenWithNoActiveGuid', async () => {
    await initializeNetworkList(fakeNetworkGuidInfoList);
    await changeActiveGuid('');
    assertFalse(!!networkListElement!.shadowRoot!.querySelector(
        ConnectivityCardElement.is));
  });

  test('SettingsLinkHiddenWhenNotLoggedIn', async () => {
    await initializeNetworkList(fakeNetworkGuidInfoList);
    assertTrue(isVisible(getSettingsLink()));
    await setIsLoggedIn(false);
    assertFalse(isVisible(getSettingsLink()));
  });

  test('RecordNavigationCalled', async () => {
    await initializeNetworkList(fakeNetworkGuidInfoList);
    assert(networkListElement);
    networkListElement.onNavigationPageChanged({isActive: false});
    await flushTasks();
    assertEquals(0, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));
    DiagnosticsBrowserProxy.setPreviousView(NavigationView.SYSTEM);
    networkListElement.onNavigationPageChanged({isActive: true});
    await flushTasks();
    assertEquals(1, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));
    assertArrayEquals(
        [NavigationView.SYSTEM, NavigationView.CONNECTIVITY],
        (DiagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
  });

  test('TastIdentifierPresent', async () => {
    await initializeNetworkList(fakeNetworkGuidInfoList);
    assert(networkListElement);
    return assertTrue(isVisible(networkListElement.shadowRoot!.querySelector(
        '.diagnostics-network-list-container')));
  });
});
