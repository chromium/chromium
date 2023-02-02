// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsPerDeviceKeyboardSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDeviceKeyboardSubsection', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardSubsectionElement}
   */
  let subsection = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    subsection = null;
  });

  function initializePerDeviceKeyboardSubsection() {
    subsection =
        document.createElement('settings-per-device-keyboard-subsection');
    document.body.appendChild(subsection);
    assertTrue(subsection != null);
    document.body.appendChild(subsection);
    return flushTasks();
  }

  test('Initialization Test', async () => {
    await initializePerDeviceKeyboardSubsection();
  });
});