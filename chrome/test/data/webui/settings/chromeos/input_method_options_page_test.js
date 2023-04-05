// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeSettingsPrivate} from './fake_settings_private.js';

/**
 * @fileoverview Suite of tests for the OS Settings input method options page.
 */
const FIRST_PARTY_INPUT_METHOD_ID_PREFIX =
    '_comp_ime_jkghodnilhceideoidjikpgommlajknk';
const PREFS_KEY = 'settings.language.input_method_specific_settings';

class FakeLanguageHelper {
  async whenReady() {}
  getInputMethodDisplayName(_) {
    return 'fake display name';
  }
  getCurrentInputMethod() {
    return Promise.resolve(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
  }
}

function getFakePrefs() {
  return [{
    key: PREFS_KEY,
    type: chrome.settingsPrivate.PrefType.DICTIONARY,
    value: {
      'xkb:us::eng': {
        physicalKeyboardAutoCorrectionLevel: 0,
        physicalKeyboardEnableCapitalization: false,
      },
    },
  }];
}

suite('InputMethodOptionsPage', function() {
  let optionsPage = null;
  let settingsPrivate = null;

  suiteSetup(async function() {
    PolymerTest.clearBody();
    CrSettingsPrefs.deferInitialization = true;
    const settingsPrefs = document.createElement('settings-prefs');
    settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    optionsPage = document.createElement('settings-input-method-options-page');
    document.body.appendChild(optionsPage);
    optionsPage.languageHelper = new FakeLanguageHelper();
    optionsPage.prefs = settingsPrefs.prefs;
  });

  /**
   * @param {string=} id Input method ID.
   */
  function createOptionsPage(id) {
    const params = new URLSearchParams();
    if (id) {
      params.append('id', id);
    }
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS, params);

    flush();
  }

  test('US English page', async () => {
    loadTimeData.overrideValues({allowPredictiveWriting: false});
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot.querySelectorAll('h2');
    assertTrue(!!titles);
    assertEquals(titles.length, 2);
    assertEquals(titles[0].textContent, 'Physical keyboard');
    assertEquals(titles[1].textContent, 'On-screen keyboard');
  });

  test('US English page from current input method', async () => {
    loadTimeData.overrideValues({allowPredictiveWriting: false});
    createOptionsPage();
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot.querySelectorAll('h2');
    assertTrue(!!titles);
    assertEquals(titles.length, 2);
    assertEquals(titles[0].textContent, 'Physical keyboard');
    assertEquals(titles[1].textContent, 'On-screen keyboard');
  });

  test('US English page with predictive writing', async () => {
    loadTimeData.overrideValues({allowPredictiveWriting: true});
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot.querySelectorAll('h2');
    assertTrue(!!titles);
    assertEquals(titles.length, 3);
    assertEquals(titles[0].textContent, 'Physical keyboard');
    assertEquals(titles[1].textContent, 'On-screen keyboard');
    assertEquals(titles[2].textContent, 'Suggestions');
  });

  test('Pinyin page', async () => {
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'zh-t-i0-pinyin');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot.querySelectorAll('h2');
    assertTrue(!!titles);
    assertEquals(titles.length, 2);
    assertEquals(titles[0].textContent, 'Advanced');
    assertEquals(titles[1].textContent, 'Physical keyboard');
  });

  test('updates options in prefs', async () => {
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const options = optionsPage.shadowRoot.querySelectorAll('.list-item');
    assertTrue(!!options);
    assertEquals(options.length, 8);
    assertEquals(
        options[0].querySelector('.start').textContent.trim(),
        'Auto-correction');
    const autoCorrectToggleButton = options[0].querySelector('cr-toggle');
    assertEquals(autoCorrectToggleButton.checked, false);
    autoCorrectToggleButton.click();
    await waitAfterNextRender(autoCorrectToggleButton);
    assertEquals(autoCorrectToggleButton.checked, true);
    assertEquals(
        optionsPage.getPref(PREFS_KEY)
            .value['xkb:us::eng']['physicalKeyboardAutoCorrectionLevel'],
        1);

    assertEquals(
        options[1].querySelector('.start').textContent.trim(),
        'Sound on keypress');
    const soundToggleButton = options[1].querySelector('cr-toggle');
    assertEquals(soundToggleButton.checked, false);
    soundToggleButton.click();
    await waitAfterNextRender(soundToggleButton);
    assertEquals(soundToggleButton.checked, true);
    assertEquals(
        optionsPage.getPref(PREFS_KEY)
            .value['xkb:us::eng']['enableSoundOnKeypress'],
        true);
  });
});
