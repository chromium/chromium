// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInputDeviceSettingsProvider} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('InputDeviceMojoInterfaceProvider', function() {
  test('SettingGettingTestProvider', () => {
    // Test that if there was no provider, getInputDeviceSettingsProvider
    // method will create one.
    const provider = getInputDeviceSettingsProvider();
    assertTrue(!!provider);
  });
});
