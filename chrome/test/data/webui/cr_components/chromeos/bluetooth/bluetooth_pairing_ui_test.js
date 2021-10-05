// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingUiElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';
import {PairingAuthType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
import {eventToPromise} from '../../../test_util.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.js';
// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothPairingUiTest', function() {
  /** @type {?SettingsBluetoothPairingUiElement} */
  let bluetoothPairingUi;

  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);

    bluetoothPairingUi = /** @type {?SettingsBluetoothPairingUiElement} */ (
        document.createElement('bluetooth-pairing-ui'));
    document.body.appendChild(bluetoothPairingUi);
    flush();
  });

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties} device
   */
  async function selectDevice(device) {
    let deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    let event = new CustomEvent('pair-device', {detail: {device}});
    deviceSelectionPage.dispatchEvent(event);
    await flushAsync();
  }

  async function simulateCancelation() {
    const event = new CustomEvent('cancel');
    const ironPages = bluetoothPairingUi.shadowRoot.querySelector('iron-pages');
    ironPages.dispatchEvent(event);
    await flushAsync();
  }

  /**
   * This function tests request PIN or request passKey UI functionality.
   * @param {!PairingAuthType} pairingAuthType
   */
  async function pairingPinOrPassKey(pairingAuthType) {
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);
    const getDeviceSelectionPage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const getDeviceRequestCodePage = () =>
        bluetoothPairingUi.shadowRoot.querySelector('#deviceRequestCodePage');
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);
    const code = '123456';

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
    await flushAsync();

    // By default device selection page should be shown.
    assertTrue(!!getDeviceSelectionPage());
    assertFalse(!!getDeviceRequestCodePage());

    // Test canceling while on request code page.
    await selectDevice(device.deviceProperties);
    let deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushAsync();

    assertTrue(!!getDeviceRequestCodePage());

    // Simulate pairing cancelation.
    await simulateCancelation();
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushAsync();

    // We return to device selection page when pairing is cancelled.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushAsync();

    // When requesting a PIN or passKey, request code page is shown.
    assertTrue(!!getDeviceRequestCodePage());
    let event = new CustomEvent('request-code-entered', {detail: {code}});
    getDeviceRequestCodePage().dispatchEvent(event);
    await flushAsync();

    assertEquals(deviceHandler.getPinOrPasskey(), code);
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushAsync();

    // We return to device selection page on pair failure.
    assertFalse(!!getDeviceRequestCodePage());

    // Retry pairing.
    await selectDevice(device.deviceProperties);
    deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.requireAuthentication(pairingAuthType);
    await flushAsync();

    event = new CustomEvent('request-code-entered', {detail: {pin: code}});
    getDeviceRequestCodePage().dispatchEvent(event);
    await flushAsync();

    // Finished event is fired on successful pairing.
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  }

  test('Device list is correctly updated', async function() {
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    assertTrue(!!deviceSelectionPage);
    assertEquals(0, deviceSelectionPage.devices.length);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushAsync();
    assertTrue(!!deviceSelectionPage.devices);
    assertEquals(1, deviceSelectionPage.devices.length);
  });

  test('finished event fired on successful device pair', async function() {
    const id = '12//345&6789';
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    assertTrue(!!deviceSelectionPage);
    let finishedPromise = eventToPromise('finished', bluetoothPairingUi);

    const device = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);

    await flushAsync();
    const event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    deviceSelectionPage.dispatchEvent(event);
    await flushAsync();

    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
    deviceHandler.completePairDevice(/*success=*/ true);
    await finishedPromise;
  });

  test('Only one device is paired at a time', async function() {
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345654321',
        /*publicName=*/ 'Head phones',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device 2',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToDiscoveredDeviceList(
        [device.deviceProperties, device1.deviceProperties]);
    await flushAsync();
    const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();

    assertEquals(deviceHandler.getPairDeviceCalledCount(), 0);

    let event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    deviceSelectionPage.dispatchEvent(event);
    await flushAsync();

    // Complete pairing to |device|.
    deviceHandler.completePairDevice(/*success=*/ false);
    await flushAsync();

    await flushAsync();
    event = new CustomEvent(
        'pair-device', {detail: {device: device.deviceProperties}});
    deviceSelectionPage.dispatchEvent(event);
    await flushAsync();

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
        const deviceSelectionPage =
            bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
        assertTrue(!!deviceSelectionPage);
        let finishedPromise = eventToPromise('finished', bluetoothPairingUi);

        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456',
            /*publicName=*/ 'BeatsX',
            /*connected=*/ true,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            mojom.AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ mojom.DeviceType.kMouse);

        bluetoothConfig.appendToDiscoveredDeviceList([device.deviceProperties]);
        await flushAsync();
        await selectDevice(device.deviceProperties);
        const deviceHandler = bluetoothConfig.getLastCreatedPairingHandler();
        deviceHandler.completePairDevice(/*success=*/ false);

        // Simulate pairing cancelation.
        await simulateCancelation();
        // Finish event is fired when canceling from device selection page.
        await finishedPromise;
      });
});
