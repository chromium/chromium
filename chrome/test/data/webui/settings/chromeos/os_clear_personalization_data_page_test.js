// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';

import {FakeSettingsPrivate} from './fake_settings_private.js';


suite('clear personalization data page', () => {
  function getFakePrefs() {
    return [];
  }

  /** @type {?settings.OsSettingsClearPersonalizedDataDialogElement} */
  let clearPersonalizedDataPage;
  /** @type {?FakeSettingsPrivate} */
  let settingsPrefs;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    document.body.innerHTML = '';
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    clearPersonalizedDataPage =
        document.createElement('os-settings-japanese-clear-ime-data-dialog');

    document.body.appendChild(clearPersonalizedDataPage);
  });

  test(
      'There exists page contents for the clear personalized data page', () => {
        assertFalse(
            clearPersonalizedDataPage.shadowRoot.querySelector('#dialogBody')
                .hidden);
        assertFalse(
            clearPersonalizedDataPage.shadowRoot.querySelector('.cancel-button')
                .hidden);
      });
});
