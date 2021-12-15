// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingConfirmCodePageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_confirm_code_page.js';
import {SettingsBluetoothPairingDeviceSelectionPageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_device_selection_page.js';
import {SettingsBluetoothPairingEnterCodeElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_enter_code_page.js';
import {SettingsBluetoothRequestCodePageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_request_code_page.js';
import {SettingsBluetoothPairingUiElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';
import {SettingsBluetoothSpinnerPageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_spinner_page.js';
import {PairingAuthType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
import {eventToPromise, flushTasks} from '../../../test_util.js';
import {waitAfterNextRender} from '../../../test_util.js';

import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.js';

// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingUiTest', function() {
  /** @type {?SettingsBluetoothPairingUiElement} */
  let bluetoothPairingUi;

  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  setup(async function() {
    bluetoothConfig = new FakeBluetoothConfig();
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  /**
   * @param {?string} pairingDeviceAddress address, when set, of the device that
   *     should directly be paired with when <bluetooth-pairing-ui> is shown.
   */
  function init(pairingDeviceAddress = null) {
    bluetoothPairingUi = /** @type {?SettingsBluetoothPairingUiElement} */ (
        document.createElement('bluetooth-pairing-ui'));
    bluetoothPairingUi.pairingDeviceAddress = pairingDeviceAddress;
    document.body.appendChild(bluetoothPairingUi);
    return flushTasks();
  }

  /**
   * @return {?SettingsBluetoothPairingDeviceSelectionPageElement}
   */
  function getDeviceSelectionPage() {
    return /** @type {?SettingsBluetoothPairingDeviceSelectionPageElement} */ (
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage'));
  }

  /**
   * @return {?SettingsBluetoothSpinnerPageElement}
   */
  function getSpinnerPage() {
    return /** @type {?SettingsBluetoothSpinnerPageElement} */ (
        bluetoothPairingUi.shadowRoot.querySelector('#spinnerPage'));
  }

  /**
   * @return {?SettingsBluetoothPairingEnterCodeElement}
   */
  function getEnterCodePage() {
    return /** @type {?SettingsBluetoothPairingEnterCodeElement} */ (
        bluetoothPairingUi.shadowRoot.querySelector('#deviceEnterCodePage'));
  }

  /**
   * @return {?SettingsBluetoothRequestCodePageElement}
   */
  function getDeviceRequestCodePage() {
    return /** @type {?SettingsBluetoothRequestCodePageElement} */ (
        bluetoothPairingUi.shadowRoot.querySelector('#deviceRequestCodePage'));
  }

  /**
   * @return {?SettingsBluetoothPairingConfirmCodePageElement}
   */
  function getConfirmCodePage() {
    return /** @type {?SettingsBluetoothPairingConfirmCodePageElement} */ (
        bluetoothPairingUi.shadowRoot.querySelector('#deviceConfirmCodePage'));
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties} device
   */
  async function selectDevice(device) {
    let event = new CustomEvent('pair-device', {detail: {device}});
    getDeviceSelectionPage().dispatchEvent(event);
    await flushTasks();
  }

  async function simulateCancelation() {
    const event = new CustomEvent('cancel');
    const ironPages = bluetoothPairingUi.shadowRoot.querySelector('iron-pages');
    ironPages.dispatchEvent(event);
    await flushTasks();
  }

  /**
   * This function tests display PIN or passKey UI functionality.
   * @param {!PairingAuthType} pairingAuthType
   */
  async function displayPinOrPasskey(pairingAuthType) {
    await init();

    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();
    const pairingCode = '123456';

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getEnterCodePage());

    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType, pairingCode);
    await flushTasks();

    assertTrue(!!getEnterCodePage());

    // Simulate pairing cancelation.
    await simulateCancelation();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    assertFalse(!!getEnterCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType, pairingCode);
    await flushTasks();

    let keyEnteredHandler = deviceHandler.getLastKeyEnteredHandlerRemote();
    keyEnteredHandler.handleKeyEntered(2);
    await flushTasks();

    assertEquals(getEnterCodePage().numKeysEntered, 2);
    assertEquals(getEnterCodePage().code, pairingCode);

    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    // Finished event is fired on successful pairing.
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  }

  /**
   * This function tests request PIN or request passKey UI functionality.
   * @param {!PairingAuthType} pairingAuthType
   */
  async function pairingPinOrPassKey(pairingAuthType) {
    await init();
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    const code = '123456';

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getDeviceRequestCodePage());

    // Test canceling while on request code page.
    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushTasks();

    assertTrue(!!getDeviceRequestCodePage());

    // Simulate pairing cancelation.
    await simulateCancelation();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushTasks();

    // When requesting a PIN or passKey, request code page is shown.
    assertTrue(!!getDeviceRequestCodePage());
    let event = new CustomEvent('request-code-entered', {detail: {code}});
    getDeviceRequestCodePage().dispatchEvent(event);
    await flushTasks();

    assertEquals(deviceHandler.getPinOrPasskey(), code);
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page on pair failure.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushTasks();

    event = new CustomEvent('request-code-entered', {detail: {pin: code}});
    getDeviceRequestCodePage().dispatchEvent(event);
    await flushTasks();

    // Finished event is fired on successful pairing.
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  }

  /**
   * Simulates opening <bluetooth-pairing-ui> with a |pairingDeviceAddress| to
   * initiate attempting to pair with a specific device.
   * @param {string} address
   */
  async function pairByDeviceAddress(address) {
    // Add the device to the unpaired devices list.
    const device = createDefaultBluetoothDevice(
        address,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    // Set BluetoothPairingUi's address with the address of the device to be
    // paired with.
    await init(/*pairingDeviceAddress=*/ address);

    // We should immediately be in the spinner page, not the device selection
    // page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    // Once we begin pairing we should still be in the spinner page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());
  }

  test('Device list is correctly updated', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(0, getDeviceSelectionPage().devices.length);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushTasks();
    assertTrue(!!getDeviceSelectionPage().devices);
    assertEquals(1, getDeviceSelectionPage().devices.length);
  });

  test('finished event fired on successful device pair', async function() {
    await init();
    const id = '12//345&6789';
    assertTrue(!!getDeviceSelectionPage());
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);

    const device = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushTasks();
    const event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage().dispatchEvent(event);
    await flushTasks();

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Only one device is paired at a time', async function() {
    await init();
    const deviceId = '123456';
    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345654321',
        /*publicName=*/ 'Head phones',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 2',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList(
        [device.deviceProperties, device1.deviceProperties]);
    await flushTasks();
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

    assertEquals(deviceHandler.getPairDeviceCalledCount(), 0);
    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, '');

    let event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage().dispatchEvent(event);
    await flushTasks();

    assertEquals(
        device.deviceProperties, getDeviceSelectionPage().devicePendingPairing);

    // Complete pairing to |device|.
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, deviceId);

    await flushTasks();
    event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage().dispatchEvent(event);
    await flushTasks();
    assertEquals(
        device.deviceProperties, getDeviceSelectionPage().devicePendingPairing);

    // pairDevice() should be called twice.
    assertEquals(deviceHandler.getPairDeviceCalledCount(), 2);
  });

  test('Request code test', async function() {
    await pairingPinOrPassKey(PairingAuthType.REQUEST_PIN_CODE);
  });

  test('Request passKey test', async function() {
    await pairingPinOrPassKey(PairingAuthType.REQUEST_PASSKEY);
  });

  test(
      'Finish event is fired when cancel is pressed in device selection page',
      async function() {
        await init();
        assertTrue(!!getDeviceSelectionPage());
        let finishedPromise = eventToPromise('finished', bluetoothPairingUi);

        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            mojom.AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ mojom.DeviceType.kMouse);

        bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
        await flushTasks();
        await selectDevice(device.deviceProperties);
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler.completePairDevice(/*success=*/ false);

        // Simulate pairing cancelation.
        await simulateCancelation();
        // Finish event is fired when canceling from device selection page.
        await finishedPromise;
      });

  test('Confirm code', async function() {
    await init();
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    const pairingCode = '123456';

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Test canceling while on confirm code page.
    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage().code, pairingCode);

    // Simulate pairing cancelation.
    await simulateCancelation();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // When Confirm code page is shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage().code, pairingCode);
    let event = new CustomEvent('confirm-code');
    getConfirmCodePage().dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());

    assertTrue(deviceHandler.getConfirmPasskeyResult());
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page on pair failure.
    assertFalse(!!getConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    event = new CustomEvent('confirm-code');
    getConfirmCodePage().dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());

    // Finished event is fired on successful pairing.
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Display PIN test', async function() {
    await displayPinOrPasskey(PairingAuthType.DISPLAY_PIN_CODE);
  });

  test('Display passkey test', async function() {
    await displayPinOrPasskey(PairingAuthType.DISPLAY_PASSKEY);
  });

  test('Pairing a new device cancels old pairing', async function() {
    await init();
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '1234321',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345654321',
        /*publicName=*/ 'Head phones',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 2',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '123454321',
        /*publicName=*/ 'Speakers',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 3',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList(
        [device.deviceProperties, device1.deviceProperties]);
    await flushTasks();
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

    // Try pairing to first device.
    await selectDevice(device.deviceProperties);
    await waitAfterNextRender(bluetoothPairingUi);

    // Try pairing to second device, before first device has completed pairing.
    await selectDevice(device1.deviceProperties);
    await waitAfterNextRender(bluetoothPairingUi);

    // Try pairing to third device, before first device has completed pairing.
    await selectDevice(device2.deviceProperties);
    await waitAfterNextRender(bluetoothPairingUi);

    // Simulate device pairing cancellation.
    deviceHandler.completePairDevice(/*success=*/ false);
    await waitAfterNextRender(bluetoothPairingUi);

    assertEquals(deviceHandler.getPairDeviceCalledCount(), 2);

    // Complete second device pairing.
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Pair with a specific device by address, success', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  // TODO(b/210128630) Fix flaky test. Closure compiler complains about using
  // test.skip() here, see
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-chromeos-rel/1058566/overview.
  // test(
  //     'Pair with a specific device by address, failure', async function() {
  //       const deviceId1 = '123456';
  //       await pairByDeviceAddress(/*address=*/ deviceId1);

  //       const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
  //       deviceHandler.completePairDevice(/*success=*/ false);

  //       // Wait for the callback to finish (flushTasks() doesn't wait long
  //       // enough here).
  //       await waitAfterNextRender(bluetoothPairingUi);

  //       // On failure, the device selection page should be shown.
  //       assertTrue(!!getDeviceSelectionPage());
  //       assertEquals(getDeviceSelectionPage().failedPairingDeviceId,
  //       deviceId1);

  //       // There should no longer be a device-specific address to pair to.
  //       assertFalse(!!bluetoothPairingUi.pairingDeviceAddress);

  //       // Verify we can pair with another device.
  //       const device2 = createDefaultBluetoothDevice(
  //           /*id=*/ '34567',
  //           /*publicName=*/ 'BeatsX',
  //           /*connectionState=*/
  //           chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
  //           /*opt_nickname=*/ 'device1',
  //           /*opt_audioCapability=*/
  //           mojom.AudioOutputCapability.kCapableOfAudioOutput,
  //           /*opt_deviceType=*/ mojom.DeviceType.kMouse);
  //       bluetoothConfig.appendToDiscoveredDeviceList(
  //           [device2.deviceProperties]);
  //       await flushTasks();

  //       let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
  //       await selectDevice(device2.deviceProperties);

  //       deviceHandler.completePairDevice(/*success=*/ true);
  //       await finishedPromise;
  //     });

  test('Pair with a specific device by address with auth', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    const pairingCode = '123457';
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage().code, pairingCode);

    // Simulate pressing 'Confirm'.
    let event = new CustomEvent('confirm-code');
    getConfirmCodePage().dispatchEvent(event);
    await waitAfterNextRender(bluetoothPairingUi);

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());
    assertTrue(deviceHandler.getConfirmPasskeyResult());

    // Finishing the pairing with success should fire the |finished| event.
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test(
      'Cancel pairing with a specific device by address with auth',
      async function() {
        await pairByDeviceAddress(/*address=*/ '123456');

        const pairingCode = '123456';
        let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler.requireAuthentication(
            PairingAuthType.CONFIRM_PASSKEY, pairingCode);
        await flushTasks();

        // Confirmation code page should be shown.
        assertTrue(!!getConfirmCodePage());
        assertEquals(getConfirmCodePage().code, pairingCode);

        // Simulate clicking 'Cancel'.
        await simulateCancelation();
        deviceHandler.completePairDevice(/*success=*/ false);
        await flushTasks();
        await waitAfterNextRender(bluetoothPairingUi);

        // The device selection page should be shown.
        assertTrue(!!getDeviceSelectionPage());

        // There should no longer be a device-specific address to pair to.
        assertFalse(!!bluetoothPairingUi.pairingDeviceAddress);
      });

  test(
      'Pair with a specific device by address with display pin code auth',
      async function() {
        await pairByDeviceAddress(/*address=*/ '123456');

        assertFalse(!!getEnterCodePage());

        const pairingCode = '123456';
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler.requireAuthentication(
            PairingAuthType.DISPLAY_PIN_CODE, pairingCode);
        await flushTasks();

        // The 'Enter Code' page should now be showing.
        assertTrue(!!getEnterCodePage());

        let keyEnteredHandler = deviceHandler.getLastKeyEnteredHandlerRemote();
        keyEnteredHandler.handleKeyEntered(2);
        await flushTasks();

        assertEquals(getEnterCodePage().numKeysEntered, 2);
        assertEquals(getEnterCodePage().code, pairingCode);

        let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
        // Finished event is fired on successful pairing.
        deviceHandler.completePairDevice(/*success=*/ true);
        await finishedPromise;
      });

  test('Cancel pairing and fail pairing ', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    const deviceId = '123456';

    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    const pairingCode = '123456';

    // Try pairing.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage().code, pairingCode);

    // Simulate pairing failure.
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // The device selection page should be shown and failed device ID
    // should be set since the pairing operation failed.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, deviceId);

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Simulate clicking 'Cancel'.
    await simulateCancelation();
    await flushTasks();

    // The device selection page should be shown, but no failed device ID
    // should be set since the operation was cancelled and did not explicitly
    // fail.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getDeviceSelectionPage().failedPairingDeviceId);
  });

  test('Disable Bluetooth during pairing', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    assertTrue(getDeviceSelectionPage().isBluetoothEnabled);

    const deviceId = '123456';
    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // Disable Bluetooth.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kDisabled);
    await flushTasks();

    // This should propagate to the device selection page.
    assertFalse(getDeviceSelectionPage().isBluetoothEnabled);

    // Re-enable and select the device.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    assertTrue(getDeviceSelectionPage().isBluetoothEnabled);
    await selectDevice(device.deviceProperties);
    await flushTasks();

    const pairingCode = '123456';
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage().code, pairingCode);

    // Disable Bluetooth.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kDisabled);
    await flushTasks();

    // We should be back to the device selection page again.
    assertFalse(!!getConfirmCodePage());
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(getDeviceSelectionPage().isBluetoothEnabled);

    // Re-enable.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    assertTrue(getDeviceSelectionPage().isBluetoothEnabled);

    // Error text shouldn't be showing because this pairing failed due to
    // Bluetooth disabling.
    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, '');

    // Select the device.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    // Simulate pairing failing.
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    // Error text should be showing.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, deviceId);

    // Disable and re-enable.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kDisabled);
    await flushTasks();

    assertFalse(getDeviceSelectionPage().isBluetoothEnabled);
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushTasks();

    assertTrue(getDeviceSelectionPage().isBluetoothEnabled);

    // Error text should no longer be showing.
    assertEquals(getDeviceSelectionPage().failedPairingDeviceId, '');
  });
});
