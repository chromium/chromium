// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_device_selection_page.js';

import type {SettingsBluetoothPairingDeviceSelectionPageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_device_selection_page.js';
import {DeviceItemState} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import type {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.js';
import {FakeBluetoothDiscoveryDelegate} from './fake_bluetooth_discovery_delegate.js';

suite('CrComponentsBluetoothPairingDeviceSelectionPageTest', function() {
  let deviceSelectionPage: SettingsBluetoothPairingDeviceSelectionPageElement;
  let bluetoothConfig: FakeBluetoothConfig;
  let discoveryDelegate: FakeBluetoothDiscoveryDelegate;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);

    deviceSelectionPage =
        document.createElement('bluetooth-pairing-device-selection-page');
    deviceSelectionPage.isBluetoothEnabled = true;
    document.body.appendChild(deviceSelectionPage);
    flush();

    discoveryDelegate = new FakeBluetoothDiscoveryDelegate();
    discoveryDelegate.addDeviceListChangedCallback(onDeviceListChanged);
    bluetoothConfig.startDiscovery(discoveryDelegate);
  });

  function onDeviceListChanged(discoveredDevices: BluetoothDeviceProperties[]) {
    deviceSelectionPage.devices = discoveredDevices;
  }

  async function flushAsync(): Promise<null> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Device lists states', async function() {
    const getDeviceList = () =>
        deviceSelectionPage.shadowRoot!.querySelector('iron-list');
    const getDeviceListTitle = () =>
        deviceSelectionPage.shadowRoot!.querySelector('#deviceListTitle');
    const getDeviceListItems = () =>
        deviceSelectionPage.shadowRoot!.querySelectorAll(
            'bluetooth-pairing-device-item');
    const getBasePage = () =>
        deviceSelectionPage.shadowRoot!.querySelector('bluetooth-base-page');
    assertTrue(getBasePage()!.showScanProgress);

    const getLearnMoreLink = () =>
        deviceSelectionPage.shadowRoot!.querySelector('localized-link');

    const learnMoreLink = getLearnMoreLink();
    assertTrue(!!learnMoreLink);
    // learnMoreLink uses ! flag because the compilar currently fails when
    // running test locally.
    assertEquals(
        learnMoreLink!.localizedString.toString(),
        deviceSelectionPage.i18nAdvanced('bluetoothPairingLearnMoreLabel')
            .toString());

    const getLearnMoreDescription = () =>
        deviceSelectionPage.shadowRoot!.querySelector(
            '#learn-more-description');
    assertFalse(!!getLearnMoreDescription());

    deviceSelectionPage.shouldOmitLinks = true;
    await flushAsync();

    assertTrue(!!getLearnMoreDescription);
    assertFalse(!!getLearnMoreLink());

    // No lists should be showing at first.
    assertFalse(!!getDeviceList());
    assertTrue(!!getDeviceListTitle());

    assertEquals(
        deviceSelectionPage.i18n('bluetoothNoAvailableDevices'),
        getDeviceListTitle()!.textContent!.trim());

    const deviceId = '12//345&6789';
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

    await flushAsync();

    const deviceList = getDeviceList();
    assertTrue(!!deviceList);
    assertEquals(deviceList!.items!.length, 1);
    assertEquals(
        deviceSelectionPage.i18n('bluetoothAvailableDevices'),
        getDeviceListTitle()!.textContent!.trim());

    let nodeList = getDeviceListItems();
    assertTrue(!!nodeList.length);
    assertEquals(nodeList[0]!.deviceItemState, DeviceItemState.DEFAULT);

    deviceSelectionPage.failedPairingDeviceId = deviceId;
    await flushAsync();

    nodeList = getDeviceListItems();
    assertTrue(nodeList.length > 0);
    assertEquals(
        getDeviceListItems()[0]!.deviceItemState, DeviceItemState.FAILED);
    await flushAsync();

    deviceSelectionPage.failedPairingDeviceId = '';
    deviceSelectionPage.devicePendingPairing = device.deviceProperties;

    await flushAsync();
    nodeList = getDeviceListItems();
    assertTrue(nodeList.length > 0);
    assertEquals(nodeList[0]!.deviceItemState, DeviceItemState.PAIRING);

    // Simulate device is pairing and is turned off.
    // Also covers case where device pairing succeeds, device list would
    // be empty but |devicePendingPairing| will still have a value.
    // this is because |devicePendingPairing| is not reset when pairing
    // succeeds.
    bluetoothConfig.resetDiscoveredDeviceList();
    deviceSelectionPage.devicePendingPairing = device.deviceProperties;
    await flushAsync();
    assertFalse(!!getDeviceList());
    assertEquals(
        deviceSelectionPage.i18n('bluetoothAvailableDevices'),
        getDeviceListTitle()!.textContent!.trim());

    // since device is turned off device pairing fails and devicePendingPairing
    // becomes null.
    deviceSelectionPage!.devicePendingPairing = null;

    await flushAsync();
    assertFalse(!!getDeviceList());
    assertEquals(
        deviceSelectionPage.i18n('bluetoothNoAvailableDevices'),
        getDeviceListTitle()!.textContent!.trim());

    // Disable Bluetooth.
    deviceSelectionPage.isBluetoothEnabled = false;
    await flushAsync();

    // Scanning progress should be hidden and the 'Bluetooth disabled' message
    // shown.
    assertFalse(getBasePage()!.showScanProgress);
    assertFalse(!!getDeviceList());
    assertEquals(
        deviceSelectionPage.i18n('bluetoothDisabled'),
        getDeviceListTitle()!.textContent!.trim());
  });

  test('Last selected item is focused', async function() {
    const getDeviceList = () =>
        deviceSelectionPage.shadowRoot!.querySelector('iron-list');
    const getDeviceListItems = () =>
        deviceSelectionPage.shadowRoot!.querySelectorAll(
            'bluetooth-pairing-device-item');

    const device1 = createDefaultBluetoothDevice(
        'deviceId1',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    const deviceId2 = 'deviceId2';
    const device2 = createDefaultBluetoothDevice(
        deviceId2,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    bluetoothConfig.appendToDiscoveredDeviceList(
        [device1.deviceProperties, device2.deviceProperties]);

    await flushAsync();
    const deviceList = getDeviceList();
    assertTrue(!!deviceList);
    // deviceList uses ! flag because the compilar currently fails when
    // running test locally.
    assertEquals(deviceList!.items!.length, 2);

    // Simulate a device being selected for pairing, then returning back to the
    // selection page.
    deviceSelectionPage.devicePendingPairing = device2.deviceProperties;
    deviceSelectionPage.devicePendingPairing = null;

    // Focus the last selected item. This should cause focus to be on the
    // second device.
    deviceSelectionPage.attemptFocusLastSelectedItem();
    await waitAfterNextRender(getDeviceList()!);

    const deviceListItems = getDeviceListItems();
    assertTrue(deviceListItems.length > 1);
    assertEquals(
        deviceListItems[1], deviceSelectionPage.shadowRoot!.activeElement);
  });
});
