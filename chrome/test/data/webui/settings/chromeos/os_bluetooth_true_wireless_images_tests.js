// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {BluetoothDeviceProperties, DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('OsBluetoothTrueWirelessImagesElementTest', function() {
  /** @type {SettingsBluetoothTrueWirelessImagesElement} */
  let bluetoothTrueWirelessImages;

  setup(function() {
    bluetoothTrueWirelessImages = /**
                            @type {?SettingsBluetoothTrueWirelessImagesElement}
                            */
        (document.createElement('os-settings-bluetooth-true-wireless-images'));
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    document.body.appendChild(bluetoothTrueWirelessImages);
    flush();
  });

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {BatteryType} batteryType
   * @param {number} batteryPercentage
   */
  async function setBatteryTypePercentage(batteryType, batteryPercentage) {
    if (!bluetoothTrueWirelessImages.device.batteryInfo) {
      bluetoothTrueWirelessImages.device.batteryInfo = {};
    }
    if (batteryType === BatteryType.DEFAULT) {
      bluetoothTrueWirelessImages.device.batteryInfo.defaultProperties = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.LEFT_BUD) {
      bluetoothTrueWirelessImages.device.batteryInfo.leftBudInfo = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.CASE) {
      bluetoothTrueWirelessImages.device.batteryInfo.caseInfo = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.RIGHT_BUD) {
      bluetoothTrueWirelessImages.device.batteryInfo.rightBudInfo = {
        batteryPercentage: batteryPercentage,
      };
    }
    bluetoothTrueWirelessImages.batteryType = batteryType;
    bluetoothTrueWirelessImages.device =
        /**
         * @type {!BluetoothDeviceProperties}
         */
        (Object.assign({}, bluetoothTrueWirelessImages.device));
    return flushAsync();
  }

  async function setTrueWirelessImages() {
    const fakeUrl = {url: 'fake_image'};
    const trueWireless = {
      leftBudImageUrl: fakeUrl,
      caseImageUrl: fakeUrl,
      rightBudImageUrl: fakeUrl,
    };
    bluetoothTrueWirelessImages.device.imageInfo = {
      defaultImageUrl: fakeUrl,
      trueWirelessImages: trueWireless,
    };

    bluetoothTrueWirelessImages.device =
        /**
         * @type {!BluetoothDeviceProperties}
         */
        (Object.assign({}, bluetoothTrueWirelessImages.device));
    return flushAsync();
  }

  test('Connected and all battery info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);

    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));

    assertEquals(
        bluetoothTrueWirelessImages.i18n('bluetoothTrueWirelessLeftBudLabel'),
        bluetoothTrueWirelessImages.shadowRoot.querySelector('#leftBudLabel')
            .innerText.trim());
    assertEquals(
        bluetoothTrueWirelessImages.i18n('bluetoothTrueWirelessCaseLabel'),
        bluetoothTrueWirelessImages.shadowRoot.querySelector('#caseLabel')
            .innerText.trim());
    assertEquals(
        bluetoothTrueWirelessImages.i18n('bluetoothTrueWirelessRightBudLabel'),
        bluetoothTrueWirelessImages.shadowRoot.querySelector('#rightBudLabel')
            .innerText.trim());
  });

  test('Connected, no battery info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });

  test('Connected and only default battery info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#notConnectedLabel'));
  });

  test('Connected and only left bud info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);

    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });

  test('Connected and only case info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });

  test('Connected and only right bud info', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });

  test('Connected and no True Wireless Images', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });

  test('Not connected state', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kNotConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;
    setTrueWirelessImages();

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertTrue(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));

    assertEquals(
        bluetoothTrueWirelessImages.i18n('bluetoothDeviceDetailDisconnected'),
        bluetoothTrueWirelessImages.shadowRoot
            .querySelector('#notConnectedLabel')
            .innerText.trim());
  });

  test('Not connected and no True Wireless Images', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kNotConnected);
    bluetoothTrueWirelessImages.device = device.deviceProperties;

    const batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);

    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#leftBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#caseContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#rightBudContainer'));
    assertFalse(!!bluetoothTrueWirelessImages.shadowRoot.querySelector(
        '#defaultContainer'));
  });
});
