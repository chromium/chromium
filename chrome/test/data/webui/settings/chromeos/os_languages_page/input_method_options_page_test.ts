// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsInputMethodOptionsPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeLanguageHelper, FIRST_PARTY_INPUT_METHOD_ID_PREFIX} from './fake_language_helper.js';

/**
 * @fileoverview Suite of tests for the OS Settings input method options page.
 */

const PREFS_KEY = 'settings.language.input_method_specific_settings';

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

function setDefaultLoadTimeData() {
  loadTimeData.overrideValues({
    // Assume by default that an admin has not disabled autocorrect.
    isPhysicalKeyboardAutocorrectAllowed: true,
    // Assume by default that the predictive writing feature is disabled.
    isPhysicalKeyboardPredictiveWritingAllowed: false,
  });
}

function setAutocorrectAllowed(value: boolean) {
  loadTimeData.overrideValues({
    isPhysicalKeyboardAutocorrectAllowed: value,
  });
}

function setPredictiveWritingAllowed(value: boolean) {
  loadTimeData.overrideValues({
    isPhysicalKeyboardPredictiveWritingAllowed: value,
  });
}

suite('<settings-input-method-options-page>', () => {
  let optionsPage: SettingsInputMethodOptionsPageElement;
  let settingsPrivate: FakeSettingsPrivate;

  suiteSetup(async () => {
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

  function createOptionsPage(id: string) {
    const params = new URLSearchParams();
    if (id) {
      params.append('id', id);
    }
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS, params);

    flush();
  }

  test('US English page', async () => {
    setDefaultLoadTimeData();
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot!.querySelectorAll('h2');
    assertEquals(2, titles.length);
    assertEquals('Physical keyboard', titles[0]!.textContent);
    assertEquals('On-screen keyboard', titles[1]!.textContent);
  });

  test('US English page from current input method', async () => {
    setDefaultLoadTimeData();
    createOptionsPage('');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot!.querySelectorAll('h2');
    assertEquals(2, titles.length);
    assertEquals('Physical keyboard', titles[0]!.textContent);
    assertEquals('On-screen keyboard', titles[1]!.textContent);
  });

  test('US English page with predictive writing', async () => {
    setDefaultLoadTimeData();
    setPredictiveWritingAllowed(true);
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot!.querySelectorAll('h2');
    assertEquals(3, titles.length);
    assertEquals('Physical keyboard', titles[0]!.textContent);
    assertEquals('On-screen keyboard', titles[1]!.textContent);
    assertEquals('Suggestions', titles[2]!.textContent);
  });

  test('US English page with autocorrect disallowed', async () => {
    setDefaultLoadTimeData();
    setAutocorrectAllowed(false);
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot!.querySelectorAll('h2');
    assertEquals(1, titles.length);
    // Note that the physical keyboard section is completely omitted if the
    // autocorrect toggle is removed. This is because the autocorrect toggle is
    // the only setting allowed for latin languages on the physical keyboard.
    assertEquals('On-screen keyboard', titles[0]!.textContent);
  });

  test('Pinyin page', async () => {
    setDefaultLoadTimeData();
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'zh-t-i0-pinyin');
    await waitAfterNextRender(optionsPage);
    const titles = optionsPage.shadowRoot!.querySelectorAll('h2');
    assertEquals(2, titles.length);
    assertEquals('Advanced', titles[0]!.textContent);
    assertEquals('Physical keyboard', titles[1]!.textContent);
  });

  test('updates options in prefs', async () => {
    setDefaultLoadTimeData();
    createOptionsPage(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
    await waitAfterNextRender(optionsPage);
    const options = optionsPage.shadowRoot!.querySelectorAll('.list-item');
    assertEquals(7, options.length);
    const autoCorrection = options[0]!.querySelector('.start');
    assertTrue(!!autoCorrection);
    assertEquals('Auto-correction', autoCorrection.textContent!.trim());
    const autoCorrectToggleButton = options[0]!.querySelector('cr-toggle');
    assertTrue(!!autoCorrectToggleButton);
    assertEquals(false, autoCorrectToggleButton.checked);
    autoCorrectToggleButton!.click();
    await waitAfterNextRender(autoCorrectToggleButton);
    assertEquals(true, autoCorrectToggleButton.checked);
    assertEquals(
        1,
        optionsPage.getPref(PREFS_KEY)
            .value['xkb:us::eng']['physicalKeyboardAutoCorrectionLevel']);

    const soundOnKeypress = options[1]!.querySelector('.start');
    assertTrue(!!soundOnKeypress);
    assertEquals('Sound on keypress', soundOnKeypress.textContent!.trim());
    const soundToggleButton = options[1]!.querySelector('cr-toggle');
    assertTrue(!!soundToggleButton);
    assertEquals(false, soundToggleButton.checked);
    soundToggleButton.click();
    await waitAfterNextRender(soundToggleButton);
    assertEquals(true, soundToggleButton.checked);
    assertEquals(
        true,
        optionsPage.getPref(PREFS_KEY)
            .value['xkb:us::eng']['enableSoundOnKeypress']);
  });
});
