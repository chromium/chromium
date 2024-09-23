// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {EditHostnameDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, DeviceNameBrowserProxyImpl, SetDeviceNameResult} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {clearBody} from '../utils.js';

import {TestDeviceNameBrowserProxy} from './test_device_name_browser_proxy.js';

suite('<edit-hostname-dialog>', () => {
  let dialog: EditHostnameDialogElement;
  let deviceNameBrowserProxy: TestDeviceNameBrowserProxy;

  function getDeviceNameInput(): CrInputElement {
    const input =
        dialog.shadowRoot!.querySelector<CrInputElement>('#deviceName');
    assertTrue(!!input);
    return input;
  }

  function getDoneButton(): HTMLButtonElement {
    const button = dialog.shadowRoot!.querySelector<HTMLButtonElement>('#done');
    assertTrue(!!button);
    return button;
  }

  function getCancelButton(): HTMLButtonElement {
    const button =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
    assertTrue(!!button);
    return button;
  }

  setup(() => {
    loadTimeData.overrideValues({
      isHostnameSettingEnabled: true,
    });

    deviceNameBrowserProxy = new TestDeviceNameBrowserProxy();
    DeviceNameBrowserProxyImpl.setInstanceForTesting(deviceNameBrowserProxy);

    clearBody();
    dialog = document.createElement('edit-hostname-dialog');
    document.body.appendChild(dialog);
  });

  function assertInput(
      value: string, invalid: boolean, valueLength: string): void {
    const inputBox = getDeviceNameInput();
    const inputCount =
        dialog.shadowRoot!.querySelector<HTMLElement>('#inputCount');
    assertTrue(!!inputCount);

    assertEquals(value, inputBox.value);
    assertEquals(invalid, inputBox.invalid);
    assertEquals(`${valueLength}/15`, inputCount.innerText.trim());

    // Done button should be disabled when input is invalid and cancel button
    // should be always enabled.
    const doneButton = getDoneButton();
    const cancelButton = getCancelButton();
    assertEquals(invalid, doneButton.disabled);
    assertFalse(cancelButton.disabled);

    // Verify A11y labels and descriptions.
    assertEquals(
        dialog.i18n('aboutDeviceNameInputA11yLabel'), inputBox.ariaLabel);
    assertEquals(
        dialog.i18n('aboutDeviceNameConstraintsA11yDescription'),
        inputBox.ariaDescription);
    assertEquals(
        dialog.i18n('aboutDeviceNameDoneBtnA11yLabel', value),
        doneButton.ariaLabel);
  }

  test('Check input sanitization and validity', () => {
    const inputBox = getDeviceNameInput();

    // Test empty name, which is the value on opening dialog.
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, under character limit.
    inputBox.value = '123456789';
    assertInput(
        /*value=*/ '123456789', /*invalid=*/ false,
        /*valueLength=*/ '9');

    // Test name with emojis, under character limit.
    inputBox.value = '1234🦤56789🧟';
    assertInput(
        /*value=*/ '123456789', /*invalid=*/ false,
        /*valueLength=*/ '9');

    // Test name with only emojis, under character limit.
    inputBox.value = '🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, at character limit.
    inputBox.value = '123456789012345';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with emojis, at character limit.
    inputBox.value = '123456789012345🧟';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with only emojis, at character limit.
    inputBox.value = '🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, above character limit.
    inputBox.value = '1234567890123456';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ true,
        /*valueLength=*/ '15');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once
    // they've reached the limit).
    inputBox.value = '12345678901234';
    assertInput(
        /*value=*/ '12345678901234', /*invalid=*/ false,
        /*valueLength=*/ '14');

    // Test name with emojis, above character limit.
    inputBox.value = '123456789012345🧟';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with only emojis, above character limit.
    inputBox.value = '🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test invalid name because of empty space character.
    inputBox.value = 'Device Name';
    assertInput(
        /*value=*/ 'Device Name', /*invalid=*/ true, /*valueLength=*/ '11');

    // Test invalid name because of special characters.
    inputBox.value = 'Device&(#@!';
    assertInput(
        /*value=*/ 'Device&(#@!', /*invalid=*/ true, /*valueLength=*/ '11');

    // Test valid name with letters and numbers.
    inputBox.value = 'Device123';
    assertInput(
        /*value=*/ 'Device123', /*invalid=*/ false, /*valueLength=*/ '9');

    // Test valid name with letters and numbers and hyphens.
    inputBox.value = '-Device1-';
    assertInput(
        /*value=*/ '-Device1-', /*invalid=*/ false, /*valueLength=*/ '9');
  });

  test('Device name can be set', async () => {
    deviceNameBrowserProxy.setDeviceNameResultForTesting(
        SetDeviceNameResult.UPDATE_SUCCESSFUL);
    const newName = 'TestName';
    getDeviceNameInput().value = newName;
    getDoneButton().click();

    await deviceNameBrowserProxy.whenCalled('attemptSetDeviceName');
    assertEquals(newName, deviceNameBrowserProxy.getDeviceName());
    assertFalse(dialog.$.dialog.open);
  });

  test('Dialog can be closed', () => {
    assertTrue(dialog.$.dialog.open);
    getCancelButton().click();
    assertFalse(dialog.$.dialog.open);
  });
});
