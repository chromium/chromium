// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {BatteryInfo, FakeInputDeviceSettingsProvider, fakeMice, PerDeviceSubsectionHeaderElement, setInputDeviceSettingsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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

  function setWelcomeExperienceFeatureState(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableWelcomeExperience: isEnabled,
    });
  }

  test('Header is visible', async () => {
    setWelcomeExperienceFeatureState(false);
    await createHeaderElement(fakeMice[0]!.batteryInfo);
    assertTrue(
        isVisible(subsectionHeader.shadowRoot!.querySelector('#deviceName')));
  });

  test('Battery info and image hidden when flag is disabled', async () => {
    setWelcomeExperienceFeatureState(false);
    await createHeaderElement(fakeMice[0]!.batteryInfo);
    const batteryInfo =
        subsectionHeader.shadowRoot!.querySelector('#batteryIcon');
    assertFalse(isVisible(batteryInfo));
  });

  test('Battery info hidden when battery info is missing', async () => {
    setWelcomeExperienceFeatureState(false);
    await createHeaderElement(fakeMice[1]!.batteryInfo);
    const batteryInfo =
        subsectionHeader.shadowRoot!.querySelector('#batteryIcon');
    assertFalse(isVisible(batteryInfo));
  });

  test('Battery info and image available', async () => {
    setWelcomeExperienceFeatureState(true);
    await createHeaderElement(
        fakeMice[0]!.batteryInfo, /*dataUrl=*/ 'data:image/png;base64,gg==');
    const batteryInfo =
        subsectionHeader.shadowRoot!.querySelector('#batteryIcon');
    assertTrue(isVisible(batteryInfo));
    assertTrue(isVisible(
        subsectionHeader!.shadowRoot!.querySelector('.device-image')));
  });

  test('Device icon displayed when image is unavailable', async () => {
    setWelcomeExperienceFeatureState(true);
    await createHeaderElement(fakeMice[0]!.batteryInfo, /*dataUrl=*/ '');
    const deviceIcon =
        subsectionHeader.shadowRoot!.querySelector('#deviceIcon');
    assertTrue(isVisible(deviceIcon));
    assertFalse(isVisible(
        subsectionHeader!.shadowRoot!.querySelector('.device-image')));
  });
});
