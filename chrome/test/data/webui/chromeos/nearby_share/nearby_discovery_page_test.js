// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/nearby_discovery_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {setDiscoveryManagerForTesting} from 'chrome://nearby/discovery_manager.js';
import {SelectShareTargetResult, ShareTargetListenerRemote, StartDiscoveryResult} from 'chrome://nearby/shared/nearby_share.mojom-webui.js';
import {ShareType} from 'chrome://nearby/shared/nearby_share_share_type.mojom-webui.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ShareTargetType} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom-webui.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';
import {isVisible} from '../test_util.js';

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
   * @param {!UnguessableToken} a
   * @param {!UnguessableToken} b
   */
  function assertTokensEqual(a, b) {
    assertEquals(a.high, b.high);
    assertEquals(a.low, b.low);
  }

  /**
   * Compares two share targets.
   * @param {?ShareTarget} a
   * @param {?ShareTarget} b
   */
  function assertShareTargetsEqual(a, b) {
    assertTrue(!!a);
    assertTrue(!!b);
    assertTokensEqual(a.id, b.id);
  }

  /**
   * Returns a list of visible share target elements.
   * @return {!Array<!NearbyDiscoveryPageElement>}
   */
  function getShareTargetElements() {
    flush();
    const selector =
        /** @type {{items: !Array<NearbyDiscoveryPageElement>}} */ (
            discoveryPageElement.shadowRoot.querySelector('.selector'));

    // If the device list isn't found, it's because the dom-if wrapping it
    // isn't showing because there are no elements.
    if (!selector) {
      return [];
    }

    return selector.items;
  }

  /**
   * Returns the |index| positioned Share Target in the concatenated Your
   * Devices and Other Devices Share Target lists.
   */
  function getNearbyDeviceElementAt(index) {
    const selfShareDevices =
        discoveryPageElement.shadowRoot.querySelector('#selfShareDevices');
    const selfShareDeviceElements =
        selfShareDevices.querySelectorAll('nearby-device');
    const nonSelfShareDevices =
        discoveryPageElement.shadowRoot.querySelector('#nonSelfShareDevices');
    const nonSelfShareDeviceElements =
        nonSelfShareDevices.querySelectorAll('nearby-device');

    assertTrue(
        selfShareDeviceElements.length + nonSelfShareDeviceElements.length >
        index);
    if (index < selfShareDeviceElements.length) {
      return selfShareDeviceElements[index];
    }
    return nonSelfShareDeviceElements[index - selfShareDeviceElements.length];
  }

  /**
   * Returns the |index|+1 positioned Share Target in the list of Your Devices
   * share targets.
   */
  function getNearbyDeviceSelfShareDevicesElementAt(index) {
    const selfShareDevices =
        discoveryPageElement.shadowRoot.querySelector('#selfShareDevices');
    const nearbyDeviceElements =
        selfShareDevices.querySelectorAll('nearby-device');
    return nearbyDeviceElements[index];
  }

  /**
   * Returns the |index|+1 positioned Share Target in the list of Non-Your
   * Devices share targets.
   */
  function getNearbyDeviceNonSelfShareDevicesElementAt(index) {
    const nonSelfShareDevices =
        discoveryPageElement.shadowRoot.querySelector('#nonSelfShareDevices');
    const nearbyDeviceElements =
        nonSelfShareDevices.querySelectorAll('nearby-device');
    return nearbyDeviceElements[index];
  }

  /**
   * Get the list of device names that are currently shown.
   * @return {!Array<string>}
   */
  function getDeviceNames() {
    const names = [];
    const shareTargets = getShareTargetElements();

    for (const shareTarget of shareTargets) {
      names.push(shareTarget.name);
    }

    return names;
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

    getNearbyDeviceElementAt(index).click();
    return true;
  }

  /**
   * Presses enter on device with |index| and returns if that succeeded.
   * @return {boolean}
   */
  function pressEnterOnDevice(index) {
    const devices = getShareTargetElements();
    if (index >= devices.length) {
      return false;
    }

    getNearbyDeviceElementAt(index).dispatchEvent(new KeyboardEvent('keydown', {
      code: 'Enter',
      key: 'Enter',
      charCode: 13,
      keyCode: 13,
      view: window,
      bubbles: true,
    }));
    return true;
  }

  /**
   * Presses space on device with |index| and returns if that succeeded.
   * @return {boolean}
   */
  function pressSpaceOnDevice(index) {
    const devices = getShareTargetElements();
    if (index >= devices.length) {
      return false;
    }

    getNearbyDeviceElementAt(index).dispatchEvent(new KeyboardEvent('keydown', {
      code: 'Space',
      key: 'Space',
      charCode: 32,
      keyCode: 32,
      view: window,
      bubbles: true,
    }));
    return true;
  }

  /**
   * Presses up arrow on device with |index| and returns if that succeeded.
   * @return {boolean}
   */
  function arrowUpOnDevice(index) {
    const devices = getShareTargetElements();
    if (index >= devices.length) {
      return false;
    }

    getNearbyDeviceElementAt(index).dispatchEvent(new KeyboardEvent('keydown', {
      code: 'ArrowUp',
      key: 'ArrowUp',
      charCode: 38,
      keyCode: 38,
      view: window,
      bubbles: true,
    }));

    return true;
  }

  /**
   * Presses down arrow on device with |index| and returns if that succeeded.
   * @return {boolean}
   */
  function arrowDownOnDevice(index) {
    const devices = getShareTargetElements();
    if (index >= devices.length) {
      return false;
    }

    getNearbyDeviceElementAt(index).dispatchEvent(new KeyboardEvent('keydown', {
      code: 'ArrowDown',
      key: 'ArrowDown',
      charCode: 40,
      keyCode: 40,
      view: window,
      composed: true,
      bubbles: true,
    }));

    return true;
  }

  /**
   * @param {!string} name Device name
   * @param {!boolean} for_self_share
   * @return {!ShareTarget}
   */
  function createShareTarget(name, for_self_share = false) {
    return {
      id: {high: BigInt(0), low: BigInt(nextId++)},
      name,
      type: ShareTargetType.kPhone,
      imageUrl: {
        url: 'testImageURL',
      },
      payloadPreview: {
        description: '',
        fileCount: 0,
        shareType: /** @type {!ShareType} */ (0),
      },
      forSelfShare: for_self_share,
    };
  }

  /**
   * @param {string} button button selector (i.e. #actionButton)
   */
  function getButton(button) {
    return discoveryPageElement.shadowRoot.querySelector('nearby-page-template')
        .shadowRoot.querySelector(button);
  }

  /**
   * Starts discovery and returns the ShareTargetListenerRemote.
   * @return {!Promise<ShareTargetListenerRemote>}
   */
  async function startDiscovery() {
    fireViewEnterStart();
    return await discoveryManager.whenCalled('startDiscovery');
  }

  /**
   * Creates a share target and sends it to the WebUI.
   * @return {!Promise<ShareTarget>}
   */
  async function setupShareTarget() {
    const listener = await startDiscovery();
    const shareTarget = createShareTarget('Device Name');
    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();
    return shareTarget;
  }

  function fireViewEnterStart() {
    discoveryPageElement.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));
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
    fireViewEnterStart();
    await discoveryManager.whenCalled('getPayloadPreview');
    assertEquals(
        discoveryManager.shareDescription,
        discoveryPageElement.shadowRoot.querySelector('nearby-preview')
            .payloadPreview.description);
  });

  test('error state with generic error', async function() {
    discoveryManager.startDiscoveryResult = StartDiscoveryResult.kErrorGeneric;
    await startDiscovery();
    flush();

    const expectedMessage = 'Something went wrong. Please try again.';
    assertEquals(
        expectedMessage,
        discoveryPageElement.shadowRoot.querySelector('#errorDescription')
            .textContent.trim());
  });

  test('error state with in progress transfer', async function() {
    discoveryManager.startDiscoveryResult =
        StartDiscoveryResult.kErrorInProgressTransferring;
    await startDiscovery();
    flush();

    const expectedMessage = 'You can only share one file at a time.' +
        ' Try again when the current transfer is complete.';
    assertEquals(
        expectedMessage,
        discoveryPageElement.shadowRoot.querySelector('#errorDescription')
            .textContent.trim());
  });

  test('error state with unavailable connections error', async function() {
    discoveryManager.startDiscoveryResult =
        StartDiscoveryResult.kNoConnectionMedium;
    await startDiscovery();
    flush();

    const expectedMessage = discoveryPageElement.i18n(
        'nearbyShareErrorNoConnectionMediumDescription');
    assertEquals(
        expectedMessage,
        discoveryPageElement.shadowRoot.querySelector('#errorDescription')
            .textContent.trim());
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
        SelectShareTargetResult.kError;

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

  test('selects share target using testing methods', async function() {
    const created = await setupShareTarget();
    const targets = discoveryPageElement.getShareTargetsForTesting();
    assertEquals(targets.length, 1);

    discoveryManager.selectShareTargetResult.token = 'test token';
    discoveryManager.selectShareTargetResult.confirmationManager =
        new FakeConfirmationManagerRemote();

    let eventDetail = null;
    discoveryPageElement.addEventListener('change-page', (event) => {
      eventDetail = event.detail;
    });

    discoveryPageElement.selectShareTargetForTesting(targets[0]);
    const selectedId = await discoveryManager.whenCalled('selectShareTarget');
    assertTokensEqual(created.id, selectedId);

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
    flush();
    const deviceList = /** @type{?HTMLElement} */
        (discoveryPageElement.shadowRoot.querySelector(
            '.device-list-container'));
    const placeholder =
        discoveryPageElement.shadowRoot.querySelector('#placeholder');
    assertTrue(!!deviceList && !deviceList.hidden);
    assertTrue(placeholder.hidden);
    assertEquals(1, getShareTargetElements().length);

    const onConnectionClosedPromise = new Promise(
        (resolve) => listener.onConnectionError.addListener(resolve));
    discoveryPageElement.dispatchEvent(new CustomEvent('view-exit-finish'));
    await onConnectionClosedPromise;

    assertFalse(!!deviceList && isVisible(deviceList));
    assertFalse(placeholder.hidden);
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

    // Click on first share target and expect it to be selected.
    assertTrue(clickOnDevice(0));
    assertShareTargetsEqual(
        targets[0], discoveryPageElement.selectedShareTarget);

    // Click on last share target and expect it to be selected.
    assertTrue(clickOnDevice(2));
    assertShareTargetsEqual(
        targets[2], discoveryPageElement.selectedShareTarget);

    const shareTarget = discoveryPageElement.selectedShareTarget;
    const onConnectionClosedPromise = new Promise(
        (resolve) => listener.onConnectionError.addListener(resolve));
    discoveryPageElement.dispatchEvent(new CustomEvent('view-exit-finish'));
    await onConnectionClosedPromise;

    // Stopping discovery does not clear selected share target.
    assertShareTargetsEqual(
        shareTarget, discoveryPageElement.selectedShareTarget);
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

  test('accessibility test for selecting device with enter', async function() {
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

    // Press enter on first share target and expect it to be selected.
    assertTrue(pressEnterOnDevice(0));
    assertShareTargetsEqual(
        targets[0], discoveryPageElement.selectedShareTarget);
  });

  test('accessibility test for selecting device with space', async function() {
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

    // Press enter on first share target and expect it to be selected.
    assertTrue(pressSpaceOnDevice(0));
    assertShareTargetsEqual(
        targets[0], discoveryPageElement.selectedShareTarget);
  });

  test(
      'accessibility test for navigating device list: down arrow',
      async function() {
        const listener = await startDiscovery();

        // Setup 3 targets to select from.
        const targets = [
          createShareTarget('Device 1'),
          createShareTarget('Device 2'),
          createShareTarget('Device 3'),
        ];
        targets.forEach((target) => listener.onShareTargetDiscovered(target));
        await listener.$.flushForTesting();

        // Press down arrow on share target and expect the next share target
        // to have focus.
        assertTrue(arrowDownOnDevice(0));
        assertEquals(getNearbyDeviceElementAt(1), getDeepActiveElement());
      });

  test(
      'accessibility test for navigating device list: up arrow',
      async function() {
        const listener = await startDiscovery();

        // Setup 3 targets to select from.
        const targets = [
          createShareTarget('Device 1'),
          createShareTarget('Device 2'),
          createShareTarget('Device 3'),
        ];
        targets.forEach((target) => listener.onShareTargetDiscovered(target));
        await listener.$.flushForTesting();

        // Press up arrow on share target and expect the previous share target
        // to have focus.
        assertTrue(arrowUpOnDevice(2));
        assertEquals(getNearbyDeviceElementAt(1), getDeepActiveElement());
      });

  test(
      'self share targets top device list when self share is enabled',
      async function() {
        const listener = await startDiscovery();

        // Add 2 non-self share targets.
        let targets = [
          createShareTarget('Device 1', /*for_self_share=*/ false),
          createShareTarget('Device 2', /*for_self_share=*/ false),
        ];
        targets.forEach((target) => listener.onShareTargetDiscovered(target));
        await listener.$.flushForTesting();

        // Add 2 self share targets.
        targets = [
          createShareTarget('Device 3', /*for_self_share=*/ true),
          createShareTarget('Device 4', /*for_self_share=*/ true),
        ];
        targets.forEach((target) => listener.onShareTargetDiscovered(target));
        await listener.$.flushForTesting();
        flush();

        assertEquals(
            getNearbyDeviceSelfShareDevicesElementAt(0).$.name.innerHTML,
            'Device 3');
        assertEquals(
            getNearbyDeviceSelfShareDevicesElementAt(1).$.name.innerHTML,
            'Device 4');
        assertEquals(
            getNearbyDeviceNonSelfShareDevicesElementAt(0).$.name.innerHTML,
            'Device 1');
        assertEquals(
            getNearbyDeviceNonSelfShareDevicesElementAt(1).$.name.innerHTML,
            'Device 2');
      });

  test('one self share target in device list', async function() {
    const listener = await startDiscovery();

    // Add a self share target.
    const target = createShareTarget('Device 1', /*for_self_share=*/ true);
    listener.onShareTargetDiscovered(target);
    await listener.$.flushForTesting();

    assertEquals(
        getNearbyDeviceSelfShareDevicesElementAt(0).$.name.innerHTML,
        'Device 1');
  });

  test('one non-self share target in device list', async function() {
    const listener = await startDiscovery();

    // Add a non-self share target.
    const target = createShareTarget('Device 1', /*for_self_share=*/ false);
    listener.onShareTargetDiscovered(target);
    await listener.$.flushForTesting();

    assertEquals(
        getNearbyDeviceNonSelfShareDevicesElementAt(0).$.name.innerHTML,
        'Device 1');
  });

  test('add and remove share targets to/from device list', async function() {
    const listener = await startDiscovery();

    // Add self, non-self share targets.
    const selfShareTarget =
        createShareTarget('self share target', /*for_self_share=*/ true);
    listener.onShareTargetDiscovered(selfShareTarget);
    const nonSelfShareTarget =
        createShareTarget('non self share target', /*for_self_share=*/ false);
    listener.onShareTargetDiscovered(nonSelfShareTarget);
    await listener.$.flushForTesting();
    assertEquals(
        getNearbyDeviceSelfShareDevicesElementAt(0).$.name.innerHTML,
        'self share target');
    assertEquals(
        getNearbyDeviceNonSelfShareDevicesElementAt(0).$.name.innerHTML,
        'non self share target');

    // Remove share targets.
    listener.onShareTargetLost(selfShareTarget);
    listener.onShareTargetLost(nonSelfShareTarget);
    await listener.$.flushForTesting();

    // Check device list length.
    const container =
        discoveryPageElement.shadowRoot.querySelector('#deviceLists');
    const nearbyDeviceElements = container.querySelectorAll('nearby-device');
    assertEquals(0, nearbyDeviceElements.length);
  });
});
