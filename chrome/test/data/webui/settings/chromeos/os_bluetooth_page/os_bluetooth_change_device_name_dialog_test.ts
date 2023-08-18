// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsBluetoothChangeDeviceNameDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement} from 'chrome://os-settings/os_settings.js';
import {getDeviceName} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<os-settings-bluetooth-change-device-name-dialog>', () => {
  let bluetoothDeviceChangeNameDialog:
      SettingsBluetoothChangeDeviceNameDialogElement;
  let bluetoothConfig: FakeBluetoothConfig;

  setup(() => {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothDeviceChangeNameDialog = document.createElement(
        'os-settings-bluetooth-change-device-name-dialog');
    document.body.appendChild(bluetoothDeviceChangeNameDialog);
    flush();
  });

  teardown(() => {
    bluetoothDeviceChangeNameDialog.remove();
  });

  /**
   * @param value The value of the input
   * @param invalid If the input is invalid or not
   * @param valueLength The length of value in string
   *     format, with 2 digits
   */
  function assertInput(
      value: string, invalid: boolean, valueLength: string): void {
    const input = bluetoothDeviceChangeNameDialog.shadowRoot!
                      .querySelector<CrInputElement>('#changeNameInput');
    const inputCount =
        bluetoothDeviceChangeNameDialog.shadowRoot!.querySelector(
            '#inputCount');
    assertTrue(!!input);
    assertTrue(!!inputCount);

    assertEquals(value, input.value);
    assertEquals(invalid, input.invalid);
    const characterCountText = bluetoothDeviceChangeNameDialog.i18n(
        'bluetoothChangeNameDialogInputCharCount', valueLength, 32);
    assertEquals(characterCountText, inputCount.textContent?.trim());
    assertEquals(
        bluetoothDeviceChangeNameDialog.i18n(
            'bluetoothChangeNameDialogInputA11yLabel', 32),
        input.ariaDescription);
  }

  test('Input is sanitized', async () => {
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.set('device', {...device1});
    await flushTasks();

    const input = bluetoothDeviceChangeNameDialog.shadowRoot!
                      .querySelector<CrInputElement>('#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);

    // Test empty name.
    input.value = '';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name, under character limit.
    input.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');


    // Test name, at character limit.
    input.value = '12345678901234567890123456789012';
    assertInput(
        /*value=*/ '12345678901234567890123456789012', /*invalid=*/ false,
        /*valueLength=*/ '32');

    // Test name, above character limit.
    input.value = '123456789012345678901234567890123';
    assertInput(
        /*value=*/ '12345678901234567890123456789012', /*invalid=*/ true,
        /*valueLength=*/ '32');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once they've
    // reached the limit).
    input.value = '1234567890123456789012345678901';
    assertInput(
        /*value=*/ '1234567890123456789012345678901', /*invalid=*/ false,
        /*valueLength=*/ '31');
  });

  test('Device name is changed', async () => {
    const id = '12//345&6789';
    const nickname = 'Nickname';
    const getDoneBtn = () => {
      const doneButton = bluetoothDeviceChangeNameDialog.shadowRoot!
                             .querySelector<HTMLButtonElement>('#done');
      assertTrue(!!doneButton);
      return doneButton;
    };

    const device = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');

    bluetoothDeviceChangeNameDialog.set('device', {...device});
    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushTasks();

    const input = bluetoothDeviceChangeNameDialog.shadowRoot!
                      .querySelector<CrInputElement>('#changeNameInput');
    assertTrue(!!input);
    assertEquals('device1', input.value);
    assertTrue(getDoneBtn().disabled);

    input.value = nickname;
    await flushTasks();
    assertFalse(getDoneBtn().disabled);

    getDoneBtn().click();
    await flushTasks();

    const newName = getDeviceName(bluetoothConfig.getPairedDeviceById(id));

    assertEquals(nickname, newName);
  });
});
