// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsPerDeviceMouseSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('PerDeviceMouseSubsection', function() {
  /**
   * @type {?SettingsPerDeviceMouseSubsectionElement}
   */
  let subsection = null;

  setup(() => {
    subsection = document.createElement('settings-per-device-mouse-subsection');
    document.body.appendChild(subsection);
  });

  teardown(() => {
    subsection = null;
  });

  test('Initialization Test', () => {
    assertTrue(subsection != null);
  });
});