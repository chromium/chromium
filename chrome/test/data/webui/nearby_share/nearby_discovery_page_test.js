// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://nearby/nearby_discovery_page.js';

import {setDiscoveryManagerForTesting} from 'chrome://nearby/discovery_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {FakeConfirmationManagerRemote, FakeDiscoveryManagerRemote} from './fake_mojo_interfaces.js';

suite('DiscoveryPageTest', function() {
  /** @type {!NearbyDiscoveryPageElement} */
  let discoveryPageElement;

  /** @type {!FakeDiscoveryManagerRemote} */
  let discoveryManager;

  /** @type {!number} Next device id to be used. */
  let nextId = 1;

  /**
   * Compares two unguessable tokens.
   * @param {!mojoBase.mojom.UnguessableToken} a
   * @param {!mojoBase.mojom.UnguessableToken} b
   */
  function assertTokensEqual(a, b) {
    assertEquals(a.high, b.high);
    assertEquals(a.low, b.low);
  }

  /**
   * Compares two share targets.
   * @param {?nearbyShare.mojom.ShareTarget} a
   * @param {?nearbyShare.mojom.ShareTarget} b
   */
  function assertShareTargetsEqual(a, b) {
    assertTrue(!!a);
    assertTrue(!!b);
    assertTokensEqual(a.id, b.id);
  }

  /**
   * Returns a list of visible share target elements. The not([hidden]) selector
   * is needed for iron-list as it reuses components but hides them when not in
   * use.
   * @return {!Array<!NearbyDiscoveryPageElement>}
   */
  function getShareTargetElements() {
    // Make sure the iron-list had time to render its elements.
    flush();
    return [...discoveryPageElement.$$('#deviceList')
                .querySelectorAll('nearby-device:not([hidden])')];
  }

  /**
   * Get the list of device names that are currently shown.
   * @return {!Array<string>}
   */
  function getDeviceNames() {
    return getShareTargetElements().map(
        (device) => device.$$('#name').textContent);
  }

  /**
   * Clicks on device with |index| and returns if that succeeded.
   * @return {boolean}
   */
  function clickOnDevice(index) {
    const devices = getShareTargetElements();
    if (index >= devices.length) {
      return false;
    }
    devices[index].click();
    return true;
  }

  /**
   * @param {!string} name Device name
   * @return {!nearbyShare.mojom.ShareTarget}
   */
  function createShareTarget(name) {
    return {
      id: {high: 0, low: nextId++},
      name,
      type: nearbyShare.mojom.ShareTargetType.kPhone,
    };
  }

  /**
   * @param {string} button button selector (i.e. #actionButton)
   */
  function getButton(button) {
    return discoveryPageElement.$$('nearby-page-template').$$(button);
  }

  /**
   * Starts discovery and returns the ShareTargetListenerRemote.
   * @return {!Promise<nearbyShare.mojom.ShareTargetListenerRemote>}
   */
  async function startDiscovery() {
    discoveryPageElement.fire('view-enter-start');
    return await discoveryManager.whenCalled('startDiscovery');
  }

  /**
   * Creates a share target and sends it to the WebUI.
   * @return {!Promise<nearbyShare.mojom.ShareTarget>}
   */
  async function setupShareTarget() {
    const listener = await startDiscovery();
    const shareTarget = createShareTarget('Device Name');
    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();
    return shareTarget;
  }

  setup(function() {
    discoveryManager = new FakeDiscoveryManagerRemote();
    setDiscoveryManagerForTesting(discoveryManager);
    discoveryPageElement = /** @type {!NearbyDiscoveryPageElement} */ (
        document.createElement('nearby-discovery-page'));
    document.body.appendChild(discoveryPageElement);
  });

  teardown(function() {
    discoveryPageElement.remove();
  });

  test('renders component', async function() {
    assertEquals('NEARBY-DISCOVERY-PAGE', discoveryPageElement.tagName);
    discoveryPageElement.fire('view-enter-start');
    await discoveryManager.whenCalled('getSendPreview');
    assertEquals(
        discoveryManager.shareDescription,
        discoveryPageElement.$$('nearby-preview').title);
  });

  test('selects share target with success', async function() {
    const created = await setupShareTarget();
    discoveryPageElement.selectedShareTarget = created;
    getButton('#actionButton').click();
    const selectedId = await discoveryManager.whenCalled('selectShareTarget');
    assertTokensEqual(created.id, selectedId);
  });

  test('selects share target with error', async function() {
    discoveryPageElement.selectedShareTarget = await setupShareTarget();
    discoveryManager.selectShareTargetResult.result =
        nearbyShare.mojom.SelectShareTargetResult.kError;

    getButton('#actionButton').click();
    await discoveryManager.whenCalled('selectShareTarget');
  });

  test('selects share target with confirmation', async function() {
    discoveryPageElement.selectedShareTarget = await setupShareTarget();
    discoveryManager.selectShareTargetResult.token = 'test token';
    discoveryManager.selectShareTargetResult.confirmationManager =
        new FakeConfirmationManagerRemote();

    let eventDetail = null;
    discoveryPageElement.addEventListener('change-page', (event) => {
      eventDetail = event.detail;
    });

    getButton('#actionButton').click();

    await discoveryManager.whenCalled('selectShareTarget');
    assertEquals('confirmation', eventDetail.page);
  });

  test('starts discovery', async function() {
    await startDiscovery();
  });

  test('stops discovery', async function() {
    const listener = await startDiscovery();
    listener.onShareTargetDiscovered(createShareTarget('Device Name'));
    await listener.$.flushForTesting();
    assertEquals(1, getShareTargetElements().length);

    const onConnectionClosedPromise = new Promise(
        (resolve) => listener.onConnectionError.addListener(resolve));
    discoveryPageElement.fire('view-exit-finish');
    await onConnectionClosedPromise;

    assertEquals(0, getShareTargetElements().length);
  });

  test('shows newly discovered device', async function() {
    const listener = await startDiscovery();
    const deviceName = 'Device Name';

    listener.onShareTargetDiscovered(createShareTarget(deviceName));
    await listener.$.flushForTesting();

    assertTrue(getDeviceNames().includes(deviceName));
  });

  test('shows multiple discovered devices', async function() {
    const listener = await startDiscovery();
    const deviceName1 = 'Device Name 1';
    const deviceName2 = 'Device Name 2';

    listener.onShareTargetDiscovered(createShareTarget(deviceName1));
    listener.onShareTargetDiscovered(createShareTarget(deviceName2));
    await listener.$.flushForTesting();

    assertTrue(getDeviceNames().includes(deviceName1));
    assertTrue(getDeviceNames().includes(deviceName2));
  });

  test('removes lost device', async function() {
    const listener = await startDiscovery();
    const deviceName = 'Device Name';
    const shareTarget = createShareTarget(deviceName);

    listener.onShareTargetDiscovered(shareTarget);
    listener.onShareTargetLost(shareTarget);
    await listener.$.flushForTesting();

    assertFalse(getDeviceNames().includes(deviceName));
  });

  test('replaces existing device', async function() {
    const listener = await startDiscovery();
    const deviceName = 'Device Name';
    const shareTarget = createShareTarget(deviceName);

    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();

    const expectedDeviceCount = getShareTargetElements().length;

    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();

    assertEquals(expectedDeviceCount, getShareTargetElements().length);
  });

  test('selects device on click', async function() {
    const listener = await startDiscovery();

    // Setup 3 targets to select from.
    const targets = [
      createShareTarget('Device 1'),
      createShareTarget('Device 2'),
      createShareTarget('Device 3'),
    ];
    targets.forEach((target) => listener.onShareTargetDiscovered(target));
    await listener.$.flushForTesting();

    assertEquals(null, discoveryPageElement.selectedShareTarget);

    // Click on fist share target and expect it to be selected.
    assertTrue(clickOnDevice(0));
    assertShareTargetsEqual(
        targets[0], discoveryPageElement.selectedShareTarget);

    // Click on last share target and expect it to be selected.
    assertTrue(clickOnDevice(2));
    assertShareTargetsEqual(
        targets[2], discoveryPageElement.selectedShareTarget);
  });

  test('loosing selected device disables next button', async function() {
    const listener = await startDiscovery();

    // Setup 3 targets and select the second one.
    const targets = [
      createShareTarget('Device 1'),
      createShareTarget('Device 2'),
      createShareTarget('Device 3'),
    ];
    targets.forEach((target) => listener.onShareTargetDiscovered(target));
    await listener.$.flushForTesting();

    assertTrue(clickOnDevice(1));
    assertFalse(getButton('#actionButton').disabled);

    // Loose the second device.
    listener.onShareTargetLost(targets[1]);
    await listener.$.flushForTesting();

    // Loosing the selected device should clear the selected device and disable
    // the next button.
    assertEquals(null, discoveryPageElement.selectedShareTarget);
    assertTrue(getButton('#actionButton').disabled);
  });

});
