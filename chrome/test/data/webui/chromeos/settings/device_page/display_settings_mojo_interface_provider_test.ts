// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDisplaySettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DisplaySettingsMojoInterfaceProvider', () => {
  test('SettingGettingTestProvider', () => {
    // Test that if there was no provider, getDisplaySettingsProvider
    // method will create one.
    const provider = getDisplaySettingsProvider();
    assertTrue(!!provider);
  });
});
