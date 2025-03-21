// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsClearBrowsingDataDialogV2Element} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('DeleteBrowsingDataDialog', function() {
  let dialog: SettingsClearBrowsingDataDialogV2Element;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-clear-browsing-data-dialog-v2');
    dialog.prefs = settingsPrefs.prefs;
    document.body.appendChild(dialog);
    return flushTasks();
  });

  test('CancelButton', async function() {
    dialog.$.cancelButton.click();
    await eventToPromise('close', dialog);
  });

  test('ShowMoreButton', function() {
    assertTrue(isVisible(dialog.$.showMoreButton));

    dialog.$.showMoreButton.click();
    assertFalse(isVisible(dialog.$.showMoreButton));
  });
});
