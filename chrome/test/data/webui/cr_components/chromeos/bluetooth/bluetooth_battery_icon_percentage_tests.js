// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';

import {BluetoothBatteryIconPercentageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_battery_icon_percentage.js';
import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {BluetoothDeviceProperties, DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';

import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';

suite('CrComponentsBluetoothBatteryIconPercentageTest', function() {
  /** @type {?BluetoothBatteryIconPercentageElement} */
  let bluetoothBatteryIconPercentage;

  setup(function() {
    bluetoothBatteryIconPercentage = /**
                            @type {?BluetoothBatteryIconPercentageElement}
                            */
        (document.createElement('bluetooth-battery-icon-percentage'));
    document.body.appendChild(bluetoothBatteryIconPercentage);
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
    bluetoothBatteryIconPercentage.device.batteryInfo = {};
    if (batteryType === BatteryType.DEFAULT) {
      bluetoothBatteryIconPercentage.device.batteryInfo.defaultProperties = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.LEFT_BUD) {
      bluetoothBatteryIconPercentage.device.batteryInfo.leftBudInfo = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.CASE) {
      bluetoothBatteryIconPercentage.device.batteryInfo.caseInfo = {
        batteryPercentage: batteryPercentage,
      };
    } else if (batteryType === BatteryType.RIGHT_BUD) {
      bluetoothBatteryIconPercentage.device.batteryInfo.rightBudInfo = {
        batteryPercentage: batteryPercentage,
      };
    }
    bluetoothBatteryIconPercentage.batteryType = batteryType;
    bluetoothBatteryIconPercentage.device =
        /**
         * @type {!BluetoothDeviceProperties}
         */
        (Object.assign({}, bluetoothBatteryIconPercentage.device));
    return flushAsync();
  }

  /**
   * @param {number} batteryPercentage
   * @param {boolean} isLowBattery
   * @param {string} batteryIconRange
   * @param {BatteryType} batteryType
   */
  function assertBatteryUIState(
      batteryPercentage, isLowBattery, batteryIconRange, batteryType) {
    let percentageText = '';
    switch (batteryType) {
      case BatteryType.LEFT_BUD:
        percentageText =
            'bluetoothPairedDeviceItemLeftBudTrueWirelessBatteryPercentage';
        break;
      case BatteryType.CASE:
        percentageText =
            'bluetoothPairedDeviceItemCaseTrueWirelessBatteryPercentage';
        break;
      case BatteryType.RIGHT_BUD:
        percentageText =
            'bluetoothPairedDeviceItemRightBudTrueWirelessBatteryPercentage';
        break;
      case BatteryType.DEFAULT:
      default:
        percentageText = 'bluetoothPairedDeviceItemBatteryPercentage';
    }
    assertEquals(
        bluetoothBatteryIconPercentage.i18n(percentageText, batteryPercentage),
        bluetoothBatteryIconPercentage.shadowRoot
            .querySelector('#batteryPercentage')
            .innerText.trim());
    assertEquals(
        isLowBattery, bluetoothBatteryIconPercentage.getIsLowBatteryForTest());
    assertTrue(!!bluetoothBatteryIconPercentage.shadowRoot.querySelector(
        '#batteryIcon'));
    assertEquals(
        'bluetooth:battery-' + batteryIconRange,
        bluetoothBatteryIconPercentage.shadowRoot.querySelector('#batteryIcon')
            .icon);
  }

  test('Battery text, icon and color', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothBatteryIconPercentage.device = device.deviceProperties;

    // Lower bound edge case.
    let batteryPercentage = 0;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage, /* isLowBattery= */ true,
        /* batteryIconRange= */ '0-7',
        /* batteryType= */ BatteryType.DEFAULT);

    batteryPercentage = 3;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ true,
        /* batteryIconRange= */ '0-7',
        /* batteryType= */ BatteryType.DEFAULT);

    // Maximum 'low battery' percentage.
    batteryPercentage = 24;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ true,
        /* batteryIconRange= */ '22-28',
        /* batteryType= */ BatteryType.DEFAULT);

    // Minimum non-'low battery' percentage.
    batteryPercentage = 25;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '22-28',
        /* batteryType= */ BatteryType.DEFAULT);

    batteryPercentage = 94;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.DEFAULT);

    // Upper bound edge case.
    batteryPercentage = 100;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.DEFAULT);

    // Broken cases where the battery doesn't show.
    batteryPercentage = 110;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertEquals(
        '',
        bluetoothBatteryIconPercentage.shadowRoot
            .querySelector('#batteryPercentage')
            .innerText.trim());
    assertEquals(
        '',
        bluetoothBatteryIconPercentage.shadowRoot.querySelector('#batteryIcon')
            .icon);
    batteryPercentage = -5;
    await setBatteryTypePercentage(BatteryType.DEFAULT, batteryPercentage);
    assertEquals(
        '',
        bluetoothBatteryIconPercentage.shadowRoot
            .querySelector('#batteryPercentage')
            .innerText.trim());
    assertEquals(
        '',
        bluetoothBatteryIconPercentage.shadowRoot.querySelector('#batteryIcon')
            .icon);
  });

  test('Custom percentage text when type labeled true', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothBatteryIconPercentage.device = device.deviceProperties;
    bluetoothBatteryIconPercentage.isTypeLabeled = true;

    const batteryPercentage = 100;

    // Left bud percentage text
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.LEFT_BUD);

    // Case percentage text
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.CASE);

    // Right bud percentage text
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.RIGHT_BUD);
  });

  test('Default percentage text when type labeled false', async function() {
    const device = createDefaultBluetoothDevice(
        /* id= */ '123456789', /* publicName= */ 'BeatsX',
        /* connectionState= */
        DeviceConnectionState.kConnected);
    bluetoothBatteryIconPercentage.device = device.deviceProperties;
    bluetoothBatteryIconPercentage.isTypeLabeled = false;

    const batteryPercentage = 100;

    // Left bud with default percentage text
    await setBatteryTypePercentage(BatteryType.LEFT_BUD, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.DEFAULT);

    // Case with default percentage text
    await setBatteryTypePercentage(BatteryType.CASE, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.DEFAULT);

    // Right bud with default percentage text
    await setBatteryTypePercentage(BatteryType.RIGHT_BUD, batteryPercentage);
    assertBatteryUIState(
        batteryPercentage,
        /* isLowBattery= */ false,
        /* batteryIconRange= */ '93-100',
        /* batteryType= */ BatteryType.DEFAULT);
  });
});
