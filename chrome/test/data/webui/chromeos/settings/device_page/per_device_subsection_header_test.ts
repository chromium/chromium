// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PerDeviceSubsectionHeaderElement} from 'chrome://os-settings/lazy_load.js';
import type {BatteryInfo} from 'chrome://os-settings/os_settings.js';
import {FakeInputDeviceSettingsProvider, fakeMice, setInputDeviceSettingsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite(PerDeviceSubsectionHeaderElement.is, () => {
  let subsectionHeader: PerDeviceSubsectionHeaderElement;
  let provider: FakeInputDeviceSettingsProvider;

  async function createHeaderElement(
      batteryInfo: BatteryInfo|null, dataUrl: string = '') {
    clearBody();
    provider = new FakeInputDeviceSettingsProvider();
    provider.setDeviceIconImage(dataUrl);
    setInputDeviceSettingsProviderForTesting(provider);
    subsectionHeader =
        document.createElement(PerDeviceSubsectionHeaderElement.is);
    subsectionHeader.batteryInfo = batteryInfo;
    subsectionHeader.name = 'device name';
    subsectionHeader.deviceKey = '0000:0001';
    document.body.appendChild(subsectionHeader);
    return flushTasks();
  }

  test('Header is visible', async () => {
    await createHeaderElement(fakeMice[0]!.batteryInfo);
    assertTrue(isVisible(
        subsectionHeader.shadowRoot!.querySelector('#deviceInfoName')));
  });

  test('Battery info hidden when battery info is missing', async () => {
    await createHeaderElement(fakeMice[1]!.batteryInfo);
    const batteryInfo =
        subsectionHeader.shadowRoot!.querySelector('#batteryIcon');
    assertFalse(isVisible(batteryInfo));
  });

  test('Battery info and image available', async () => {
    await createHeaderElement(
        fakeMice[0]!.batteryInfo, /*dataUrl=*/ 'data:image/png;base64,gg==');
    const batteryInfo =
        subsectionHeader!.shadowRoot!.querySelector('#batteryIcon');
    assertTrue(isVisible(batteryInfo));
    assertTrue(
        isVisible(subsectionHeader.shadowRoot!.querySelector('.device-image')));
  });

  test('Device icon displayed when image is unavailable', async () => {
    await createHeaderElement(fakeMice[0]!.batteryInfo, /*dataUrl=*/ '');
    const deviceIcon =
        subsectionHeader.shadowRoot!.querySelector('#deviceIcon');
    assertTrue(isVisible(deviceIcon));
    assertFalse(isVisible(
        subsectionHeader!.shadowRoot!.querySelector('.device-image')));
  });
});
