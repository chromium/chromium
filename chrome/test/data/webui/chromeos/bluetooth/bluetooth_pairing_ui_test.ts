// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_ui.js';

import type {SettingsBluetoothPairingConfirmCodePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_confirm_code_page.js';
import type {SettingsBluetoothPairingDeviceSelectionPageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_device_selection_page.js';
import type {SettingsBluetoothPairingEnterCodeElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_enter_code_page.js';
import type {SettingsBluetoothRequestCodePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_request_code_page.js';
import type {SettingsBluetoothPairingUiElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_ui.js';
import type {SettingsBluetoothSpinnerPageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_spinner_page.js';
import {PairingAuthType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {AudioOutputCapability, BluetoothSystemState, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import type {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.js';

import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothPairingUiTest', function() {
  let bluetoothPairingUi: SettingsBluetoothPairingUiElement;
  let bluetoothConfig: FakeBluetoothConfig;

  setup(async function() {
    bluetoothConfig = new FakeBluetoothConfig();
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  /**
   * @param {?string} pairingDeviceAddress address, when set, of the device that
   *     should directly be paired with when <bluetooth-pairing-ui> is shown.
   */
  function init(pairingDeviceAddress?: string) {
    bluetoothPairingUi = document.createElement('bluetooth-pairing-ui');
    bluetoothPairingUi.pairingDeviceAddress = pairingDeviceAddress || '';
    document.body.appendChild(bluetoothPairingUi);
    return flushTasks();
  }

  function getDeviceSelectionPage():
      SettingsBluetoothPairingDeviceSelectionPageElement|null {
    return bluetoothPairingUi.shadowRoot!.querySelector('#deviceSelectionPage');
  }

  function getSpinnerPage(): SettingsBluetoothSpinnerPageElement|null {
    return bluetoothPairingUi.shadowRoot!.querySelector('#spinnerPage');
  }

  function getEnterCodePage(): SettingsBluetoothPairingEnterCodeElement|null {
    return bluetoothPairingUi.shadowRoot!.querySelector('#deviceEnterCodePage');
  }

  function getDeviceRequestCodePage(): SettingsBluetoothRequestCodePageElement|
      null {
    return bluetoothPairingUi.shadowRoot!.querySelector(
        '#deviceRequestCodePage');
  }

  function getConfirmCodePage(): SettingsBluetoothPairingConfirmCodePageElement|
      null {
    return bluetoothPairingUi.shadowRoot!.querySelector(
        '#deviceConfirmCodePage');
  }

  async function selectDevice(device: BluetoothDeviceProperties):
      Promise<void> {
    const event = new CustomEvent('pair-device', {detail: {device}});
    getDeviceSelectionPage()!.dispatchEvent(event);
    await flushTasks();
  }

  async function simulateCancelation(): Promise<void> {
    const event = new CustomEvent('cancel');
    const ironPages =
        bluetoothPairingUi.shadowRoot!.querySelector<HTMLElement>('iron-pages');
    ironPages!.dispatchEvent(event);
    await flushTasks();

    // Explicitly fail the pairing.
    bluetoothConfig.getLastCreatedPairingHandler()!.completePairDevice(
        /*success=*/ false);
    await flushTasks();
  }

  async function displayPinOrPasskey(pairingAuthType: PairingAuthType):
      Promise<void> {
    await init();

    const deviceName = 'BeatsX';
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ deviceName,
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();
    const pairingCode = '123456';

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getEnterCodePage());

    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    // deviceHandler uses ! flag because the compilar currently fails when
    // running test locally.
    deviceHandler!.requireAuthentication(pairingAuthType, pairingCode);
    await flushTasks();

    assertTrue(!!getEnterCodePage());
    assertEquals(getEnterCodePage()!.deviceName, deviceName);

    // Simulate pairing cancelation.
    await simulateCancelation();

    assertFalse(!!getEnterCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(pairingAuthType, pairingCode);
    await flushTasks();

    const keyEnteredHandler = deviceHandler!.getLastKeyEnteredHandlerRemote();
    keyEnteredHandler.handleKeyEntered(2);
    await flushTasks();

    assertEquals(getEnterCodePage()!.numKeysEntered, 2);
    assertEquals(getEnterCodePage()!.code, pairingCode);
    assertEquals(getEnterCodePage()!.deviceName, deviceName);

    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    // Finished event is fired on successful pairing.
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  }

  /**
   * This function tests request PIN or request passKey UI functionality.
   */
  async function pairingPinOrPassKey(pairingAuthType: PairingAuthType):
      Promise<void> {
    await init();
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    const code = '123456';

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getDeviceRequestCodePage());

    // Test canceling while on request code page.
    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(pairingAuthType);
    await flushTasks();

    assertTrue(!!getDeviceRequestCodePage());

    // Simulate pairing cancelation.
    await simulateCancelation();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(pairingAuthType);
    await flushTasks();

    // When requesting a PIN or passKey, request code page is shown.
    assertTrue(!!getDeviceRequestCodePage());
    const eventCode = new CustomEvent('request-code-entered', {detail: {code}});
    getDeviceRequestCodePage()!.dispatchEvent(eventCode);
    await flushTasks();

    assertEquals(deviceHandler!.getPinOrPasskey(), code);
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page on pair failure.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(pairingAuthType);
    await flushTasks();

    const eventPin =
        new CustomEvent('request-code-entered', {detail: {pin: code}});
    getDeviceRequestCodePage()!.dispatchEvent(eventPin);
    await flushTasks();

    // Finished event is fired on successful pairing.
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  }

  /**
   * Simulates opening <bluetooth-pairing-ui> with a |pairingDeviceAddress| to
   * initiate attempting to pair with a specific device.
   */
  async function pairByDeviceAddress(address: string): Promise<void> {
    // Add the device to the unpaired devices list.
    const device = createDefaultBluetoothDevice(
        address,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    // Set BluetoothPairingUi's address with the address of the device to be
    // paired with.
    await init(/*pairingDeviceAddress=*/ address);

    // We should immediately be in the spinner page, not the device selection
    // page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    await deviceHandler!.completeFetchDevice(device.deviceProperties);

    // Wait for DevicePairingHandler.PairDevice() to be called.
    await deviceHandler!.waitForPairDevice();

    // Once we begin pairing we should still be in the spinner page.
    assertTrue(!!getSpinnerPage());
    assertFalse(!!getDeviceSelectionPage());
  }

  test('Device list is correctly updated', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(0, getDeviceSelectionPage()!.devices.length);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushTasks();
    assertTrue(!!getDeviceSelectionPage()!.devices);
    assertEquals(1, getDeviceSelectionPage()!.devices.length);
  });

  test('finished event fired on successful device pair', async function() {
    await init();
    const id = '12//345&6789';
    assertTrue(!!getDeviceSelectionPage());
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);

    const device = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushTasks();
    const event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage()!.dispatchEvent(event);
    await flushTasks();

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Only one device is paired at a time', async function() {
    await init();
    const deviceId = '123456';
    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345654321',
        /*publicName=*/ 'Head phones',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 2',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList(
        [device.deviceProperties, device1.deviceProperties]);
    await flushTasks();
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

    assertEquals(deviceHandler!.getPairDeviceCalledCount(), 0);
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, '');

    let event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage()!.dispatchEvent(event);
    await flushTasks();

    assertEquals(
        device.deviceProperties,
        getDeviceSelectionPage()!.devicePendingPairing);

    // Complete pairing to |device|.
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();

    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, deviceId);

    await flushTasks();
    event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    getDeviceSelectionPage()!.dispatchEvent(event);
    await flushTasks();
    assertEquals(
        device.deviceProperties,
        getDeviceSelectionPage()!.devicePendingPairing);

    // pairDevice() should be called twice.
    assertEquals(deviceHandler!.getPairDeviceCalledCount(), 2);
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
        const finishedPromise = eventToPromise('finished', bluetoothPairingUi);

        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
        await flushTasks();
        await selectDevice(device.deviceProperties);
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler!.completePairDevice(/*success=*/ false);
        await flushTasks();

        // Simulate clicking cancel button.
        await simulateCancelation();
        // Finish event is fired when canceling from device selection page.
        await finishedPromise;
      });

  test('Cancel pairing without completing pairing', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();
    await selectDevice(device.deviceProperties);
    await flushTasks();

    // Cancel pairing before it finishes, this should cancel pairing.
    await simulateCancelation();

    // Clicking cancel again should close the UI.
    await simulateCancelation();
    // Finish event is fired when canceling from device selection page.
    await finishedPromise;
  });

  test('Confirm code', async function() {
    await init();
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
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
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage()!.code, pairingCode);

    // Simulate pairing cancelation.
    await simulateCancelation();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // When Confirm code page is shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage()!.code, pairingCode);
    let event = new CustomEvent('confirm-code');
    getConfirmCodePage()!.dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());

    assertTrue(deviceHandler!.getConfirmPasskeyResult());
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();

    // We return to device selection page on pair failure.
    assertFalse(!!getConfirmCodePage());
    assertFalse(!!getSpinnerPage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    event = new CustomEvent('confirm-code');
    getConfirmCodePage()!.dispatchEvent(event);
    await flushTasks();

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());

    // Finished event is fired on successful pairing.
    deviceHandler!.completePairDevice(/*success=*/ true);
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
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const device = createDefaultBluetoothDevice(
        /*id=*/ '1234321',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345654321',
        /*publicName=*/ 'Head phones',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 2',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '123454321',
        /*publicName=*/ 'Speakers',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device 3',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList(
        [device.deviceProperties, device1.deviceProperties]);
    await flushTasks();
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

    // Try pairing to first device.
    let pairDevicePromise = deviceHandler!.waitForPairDevice();
    await selectDevice(device.deviceProperties);

    // Wait for DevicePairingHandler.PairDevice() to be called.
    await pairDevicePromise;
    assertEquals(deviceHandler!.getPairDeviceCalledCount(), 1);

    // Try pairing to second device, before first device has completed pairing.
    await selectDevice(device1.deviceProperties);
    await waitAfterNextRender(bluetoothPairingUi);

    // Try pairing to third device, before first device has completed pairing.
    await selectDevice(device2.deviceProperties);
    await waitAfterNextRender(bluetoothPairingUi);

    // Simulate device pairing cancellation.
    pairDevicePromise = deviceHandler!.waitForPairDevice();
    deviceHandler!.completePairDevice(/*success=*/ false);

    // Wait for DevicePairingHandler.PairDevice() to be called.
    await pairDevicePromise;
    assertEquals(deviceHandler!.getPairDeviceCalledCount(), 2);

    // Complete second device pairing.
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test(
      'Do not pair queued device if handler becomes unavailable',
      async function() {
        await init();
        const device = createDefaultBluetoothDevice(
            /*id=*/ '1234321',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device 1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        const device1 = createDefaultBluetoothDevice(
            /*id=*/ '12345654321',
            /*publicName=*/ 'Head phones',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device 2',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        const device2 = createDefaultBluetoothDevice(
            /*id=*/ '12345555554321',
            /*publicName=*/ 'Speakers',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device 3',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothConfig.appendToDiscoveredDeviceList([
          device.deviceProperties,
          device1.deviceProperties,
          device2.deviceProperties,
        ]);
        await flushTasks();
        let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

        // Try pairing to first device.
        let pairDevicePromise = deviceHandler!.waitForPairDevice();
        await selectDevice(device.deviceProperties);

        // Wait for DevicePairingHandler.PairDevice() to be called.
        await pairDevicePromise;
        assertEquals(deviceHandler!.getPairDeviceCalledCount(), 1);

        // Try pairing to second device, before first device has completed
        // pairing.
        await selectDevice(device1.deviceProperties);
        await waitAfterNextRender(bluetoothPairingUi);

        // Disable Bluetooth.
        bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
        await flushTasks();

        assertFalse(getDeviceSelectionPage()!.isBluetoothEnabled);

        // Simulate device pairing cancellation.
        pairDevicePromise = deviceHandler!.waitForPairDevice();
        deviceHandler!.completePairDevice(/*success=*/ false);
        assertEquals(deviceHandler!.getPairDeviceCalledCount(), 1);

        // New pairing handler would be null, since Bluetooth is disabled.
        deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        assertFalse(!!deviceHandler);

        // Test to make sure device pending pairing is reset to null, this
        // should be the case because device pairing handler is null.

        // Re-enable and select the device.
        const onBluetoothDiscoveryStartedPromise =
            bluetoothPairingUi.waitForOnBluetoothDiscoveryStartedForTest();
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);

        // Wait for |devicePairingHandler_| to be set in
        // onBluetoothDiscoveryStarted().
        await onBluetoothDiscoveryStartedPromise;

        deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        assertTrue(!!deviceHandler);

        // Try pairing to device.
        pairDevicePromise = deviceHandler!.waitForPairDevice();
        await selectDevice(device2.deviceProperties);
        await pairDevicePromise;
        assertEquals(deviceHandler!.getPairDeviceCalledCount(), 1);

        // Simulate device pairing cancellation and make sure there are
        // no queued pairing devices.
        deviceHandler!.completePairDevice(/*success=*/ false);
        await flushTasks();
        await waitAfterNextRender(bluetoothPairingUi);
        assertEquals(deviceHandler!.getPairDeviceCalledCount(), 1);
      });

  test('Pair with a specific device by address, success', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test(
      'Cancel after attempting to pair to a device with address not found',
      async function() {
        await init(/*pairingDeviceAddress=*/ '123456');

        // We should immediately be in the spinner page, not the device
        // selection page.
        assertTrue(!!getSpinnerPage());
        assertFalse(!!getDeviceSelectionPage());

        // Return no device.
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        await deviceHandler!.completeFetchDevice(null);

        // Pairing is not initialized since device does not exit in discoverable
        // devices list.
        assertFalse(!!deviceHandler!.getLastPairingDelegate());

        const finishedPromise = eventToPromise('finished', bluetoothPairingUi);

        // Simulate clicking 'Cancel'.
        const event = new CustomEvent('cancel');
        const ironPages =
            bluetoothPairingUi.shadowRoot!.querySelector('iron-pages');
        ironPages!.dispatchEvent(event);
        await finishedPromise;
      });

  test('Pair with a specific device by address, failure', async function() {
    const deviceId1 = '123456';
    await pairByDeviceAddress(/*address=*/ deviceId1);

    const handlePairDeviceResultPromise =
        bluetoothPairingUi.waitForHandlePairDeviceResultForTest();
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.completePairDevice(/*success=*/ false);
    await handlePairDeviceResultPromise;

    // On failure, the device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, deviceId1);

    // There should no longer be a device-specific address to pair to.
    assertFalse(!!bluetoothPairingUi.pairingDeviceAddress);

    // Verify we can pair with another device.
    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '34567',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device2.deviceProperties]);
    await flushTasks();

    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    await selectDevice(device2.deviceProperties);

    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Pair with a specific device by address with auth', async function() {
    await pairByDeviceAddress(/*address=*/ '123456');

    const pairingCode = '123457';
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage()!.code, pairingCode);

    // Simulate pressing 'Confirm'.
    const event = new CustomEvent('confirm-code');
    const finishRequestConfirmPasskeyPromise =
        deviceHandler!.waitForFinishRequestConfirmPasskey();
    getConfirmCodePage()!.dispatchEvent(event);

    // Wait for confirm passkey result to propagate to device handler.
    await finishRequestConfirmPasskeyPromise;

    // Spinner should be shown.
    assertTrue(!!getSpinnerPage());
    assertTrue(deviceHandler!.getConfirmPasskeyResult());

    // Finishing the pairing with success should fire the |finished| event.
    const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    deviceHandler!.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test(
      'Cancel pairing with a specific device by address with auth',
      async function() {
        await pairByDeviceAddress(/*address=*/ '123456');

        const pairingCode = '123456';
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler!.requireAuthentication(
            PairingAuthType.CONFIRM_PASSKEY, pairingCode);
        await flushTasks();

        // Confirmation code page should be shown.
        assertTrue(!!getConfirmCodePage());
        assertEquals(getConfirmCodePage()!.code, pairingCode);

        // Simulate clicking 'Cancel'.
        await simulateCancelation();
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
        deviceHandler!.requireAuthentication(
            PairingAuthType.DISPLAY_PIN_CODE, pairingCode);
        await flushTasks();

        // The 'Enter Code' page should now be showing.
        assertTrue(!!getEnterCodePage());

        const keyEnteredHandler =
            deviceHandler!.getLastKeyEnteredHandlerRemote();
        keyEnteredHandler.handleKeyEntered(2);
        await flushTasks();

        assertEquals(getEnterCodePage()!.numKeysEntered, 2);
        assertEquals(getEnterCodePage()!.code, pairingCode);

        const finishedPromise = eventToPromise('finished', bluetoothPairingUi);
        // Finished event is fired on successful pairing.
        deviceHandler!.completePairDevice(/*success=*/ true);
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
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    const pairingCode = '123456';

    // Try pairing.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage()!.code, pairingCode);

    let attemptFocusLastSelectedItemCallCount = 0;
    getDeviceSelectionPage()!.attemptFocusLastSelectedItem = () => {
      attemptFocusLastSelectedItemCallCount++;
    };

    // Simulate pairing failure.
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();

    // The device selection page should be shown and failed device ID
    // should be set since the pairing operation failed. The device list item
    // should be focused.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, deviceId);
    assertEquals(1, attemptFocusLastSelectedItemCallCount);

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Simulate clicking 'Cancel'.
    await simulateCancelation();

    // The device selection page should be shown, but no failed device ID
    // should be set since the operation was cancelled and did not explicitly
    // fail. The device list item should be focused.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getDeviceSelectionPage()!.failedPairingDeviceId);
    assertEquals(2, attemptFocusLastSelectedItemCallCount);
  });

  test('Disable Bluetooth during pairing', async function() {
    await init();
    assertTrue(!!getDeviceSelectionPage());
    assertTrue(getDeviceSelectionPage()!.isBluetoothEnabled);

    const deviceId = '123456';
    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // Disable Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    // This should propagate to the device selection page.
    assertFalse(getDeviceSelectionPage()!.isBluetoothEnabled);

    // Re-enable and select the device.
    let onBluetoothDiscoveryStartedPromise =
        bluetoothPairingUi.waitForOnBluetoothDiscoveryStartedForTest();
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);

    // Wait for |devicePairingHandler_| to be set in
    // onBluetoothDiscoveryStarted().
    await onBluetoothDiscoveryStartedPromise;

    assertTrue(getDeviceSelectionPage()!.isBluetoothEnabled);
    await selectDevice(device.deviceProperties);
    await flushTasks();

    const pairingCode = '123456';
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.requireAuthentication(
        PairingAuthType.CONFIRM_PASSKEY, pairingCode);
    await flushTasks();

    // Confirmation code page should be shown.
    assertTrue(!!getConfirmCodePage());
    assertEquals(getConfirmCodePage()!.code, pairingCode);

    // Disable Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    // We should be back to the device selection page again.
    assertFalse(!!getConfirmCodePage());
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(getDeviceSelectionPage()!.isBluetoothEnabled);

    // Re-enable.
    onBluetoothDiscoveryStartedPromise =
        bluetoothPairingUi.waitForOnBluetoothDiscoveryStartedForTest();
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);

    // Wait for |devicePairingHandler_| to be set in
    // onBluetoothDiscoveryStarted().
    await onBluetoothDiscoveryStartedPromise;

    assertTrue(getDeviceSelectionPage()!.isBluetoothEnabled);

    // Error text shouldn't be showing because this pairing failed due to
    // Bluetooth disabling.
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, '');

    // Select the device.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    // Simulate pairing failing.
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    // Error text should be showing.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, deviceId);

    // Disable Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    assertFalse(getDeviceSelectionPage()!.isBluetoothEnabled);

    // Re-enable Bluetooth.
    onBluetoothDiscoveryStartedPromise =
        bluetoothPairingUi.waitForOnBluetoothDiscoveryStartedForTest();
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await onBluetoothDiscoveryStartedPromise;

    assertTrue(getDeviceSelectionPage()!.isBluetoothEnabled);

    // Error text should no longer be showing.
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, '');
  });

  test('Error message is not preserved', async function() {
    // Test to ensure error message is not preserved if pairing fails and
    // device is removed and readded to device list.

    await init();
    assertTrue(!!getDeviceSelectionPage());
    assertTrue(getDeviceSelectionPage()!.isBluetoothEnabled);

    const deviceId = '123456';
    const device = createDefaultBluetoothDevice(
        deviceId,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // Select the device.
    await selectDevice(device.deviceProperties);
    await flushTasks();

    // Simulate pairing failing.
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler!.completePairDevice(/*success=*/ false);
    await flushTasks();
    await waitAfterNextRender(bluetoothPairingUi);

    // Error text should be showing.
    assertTrue(!!getDeviceSelectionPage());
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, deviceId);

    // Reset device list.
    bluetoothConfig.resetDiscoveredDeviceList();
    await flushTasks();

    // Add device back.
    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushTasks();

    // Error text should no longer be showing.
    assertEquals(getDeviceSelectionPage()!.failedPairingDeviceId, '');
  });

  // Regression test for b/231738454.
  test(
      'Mojo connections are closed after dialog is removed from DOM',
      async function() {
        await init();
        assertEquals(1, bluetoothConfig.getNumStartDiscoveryCalls());

        // Remove the pairing dialog from the DOM.
        bluetoothPairingUi.remove();

        // Disable Bluetooth.
        bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
        await flushTasks();

        // Re-enable Bluetooth. If the Mojo connections are still alive, this
        // will trigger discovery to start again.
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        await flushTasks();
        assertEquals(1, bluetoothConfig.getNumStartDiscoveryCalls());
      });
});
