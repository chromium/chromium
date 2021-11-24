// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingUiElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';
import {PairingAuthType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
import {eventToPromise, flushTasks} from '../../../test_util.js';
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
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties} device
   */
  async function selectDevice(device) {
    let deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    let event = new CustomEvent('pair-device', {detail: {device}});
    deviceSelectionPage.dispatchEvent(event);
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
    const getDeviceSelectionPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const getEnterCodePage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceEnterCodePage');

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
    const getDeviceSelectionPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const getDeviceRequestCodePage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceRequestCodePage');
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

    const getDeviceSelectionPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const getSpinnerPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#spinnerPage');

    // We should immediately be in the spinner page, not the device selection
    // page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());
    await flushTasks();

    // Once we begin pairing we should still be in the spinner page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());
  }

  test('Device list is correctly updated', async function() {
    await init();
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    assertTrue(!!deviceSelectionPage);
    assertEquals(0, deviceSelectionPage.devices.length);

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
    assertTrue(!!deviceSelectionPage.devices);
    assertEquals(1, deviceSelectionPage.devices.length);
  });

  test('finished event fired on successful device pair', async function() {
    await init();
    const id = '12//345&6789';
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    assertTrue(!!deviceSelectionPage);
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
    deviceSelectionPage.dispatchEvent(event);
    await flushTasks();

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Only one device is paired at a time', async function() {
    await init();
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
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
    assertEquals(deviceSelectionPage.failedPairingDeviceId, '');

    let event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    deviceSelectionPage.dispatchEvent(event);
    await flushTasks();

    assertEquals(
        device.deviceProperties, deviceSelectionPage.devicePendingPairing);

    // Complete pairing to |device|.
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    assertEquals(deviceSelectionPage.failedPairingDeviceId, deviceId);

    await flushTasks();
    event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    deviceSelectionPage.dispatchEvent(event);
    await flushTasks();
    assertEquals(
        device.deviceProperties, deviceSelectionPage.devicePendingPairing);

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
        const deviceSelectionPage =
            bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
        assertTrue(!!deviceSelectionPage);
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
    const getDeviceSelectionPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const getDeviceConfirmCodePage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceConfirmCodePage');
    const getSpinnerPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#spinnerPage');
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
    assertFalse(!!getDeviceConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Test canceling while on confirm code page.
    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    assertTrue(!!getDeviceConfirmCodePage());
    assertEquals(getDeviceConfirmCodePage().code, pairingCode);

    // Simulate pairing cancelation.
    await simulateCancelation();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getDeviceConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // When Confirm code page is shown.
    assertTrue(!!getDeviceConfirmCodePage());
    assertEquals(getDeviceConfirmCodePage().code, pairingCode);
    let event = new CustomEvent('confirm-code');
    getDeviceConfirmCodePage().dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());

    assertTrue(deviceHandler.getConfirmPasskeyResult());
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page on pair failure.
    assertFalse(!!getDeviceConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    event = new CustomEvent('confirm-code');
    getDeviceConfirmCodePage().dispatchEvent(event);
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
    await flushTasks();

    // Try pairing to second device, before first device has completed pairing.
    await selectDevice(device1.deviceProperties);
    await flushTasks();

    // Try pairing to third device, before first device has completed pairing.
    await selectDevice(device2.deviceProperties);
    await flushTasks();

    // Simulate device pairing cancellation.
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushTasks();

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

  test('Pair with a specific device by address, failure', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ false);

    // TODO(crbug.com/1010321): Right now we will still be in the spinner
    // page but this should be updated to check we're in the error page.
    assertTrue(!!bluetoothPairingUi.shadowRoot.querySelector('#spinnerPage'));
  });

  test('Pair with a specific device by address with auth', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    const pairingCode = '123457';
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    const getDeviceConfirmCodePage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceConfirmCodePage');
    assertTrue(!!getDeviceConfirmCodePage());
    assertEquals(getDeviceConfirmCodePage().code, pairingCode);

    // Simulate pressing 'Confirm'.
    let event = new CustomEvent('confirm-code');
    getDeviceConfirmCodePage().dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!bluetoothPairingUi.shadowRoot.querySelector('#spinnerPage'));
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
        const getDeviceConfirmCodePage = () =>
            bluetoothPairingUi.shadowRoot.querySelector(
                '#deviceConfirmCodePage');
        assertTrue(!!getDeviceConfirmCodePage());
        assertEquals(getDeviceConfirmCodePage().code, pairingCode);

        // Simulate clicking 'Cancel'. The |finished| event should fire.
        let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
        await simulateCancelation();
        await finishedPromise;
      });

  test(
      'Pair with a specific device by address with display pin code auth',
      async function() {
        await pairByDeviceAddress(/*address=*/ '123456');

        const getEnterCodePage = () =>
            bluetoothPairingUi.shadowRoot.querySelector('#deviceEnterCodePage');
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
});
