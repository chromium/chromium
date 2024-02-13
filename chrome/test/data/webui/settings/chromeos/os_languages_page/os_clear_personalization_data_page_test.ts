// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsClearPersonalizedDataDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSettingsPrefs, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

suite('<os-settings-japanese-clear-ime-data-dialog>', () => {
  function getFakePrefs() {
    return [];
  }

  let clearPersonalizedDataPage: OsSettingsClearPersonalizedDataDialogElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    clearPersonalizedDataPage =
        document.createElement('os-settings-japanese-clear-ime-data-dialog');

    document.body.appendChild(clearPersonalizedDataPage);
  });

  test(
      'There exists page contents for the clear personalized data page', () => {
        const dialogBody =
            clearPersonalizedDataPage.shadowRoot!.querySelector<HTMLDivElement>(
                '#dialogBody');
        assertTrue(!!dialogBody);
        assertFalse(dialogBody.hidden);

        const cancelButton =
            clearPersonalizedDataPage.shadowRoot!
                .querySelector<CrButtonElement>('.cancel-button');
        assertTrue(!!cancelButton);
        assertFalse(cancelButton.hidden);
      });
});
