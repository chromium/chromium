// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, isWindows} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {FakeLanguageSettingsPrivate} from 'chrome://test/settings/fake_language_settings_private.js';
import {FakeSettingsPrivate} from 'chrome://test/settings/fake_settings_private.js';
import {TestLanguagesBrowserProxy} from 'chrome://test/settings/test_languages_browser_proxy.js';

// clang-format on

suite('settings-edit-dictionary-page', function() {
  function getFakePrefs() {
    const fakePrefs = [
      {
        key: 'intl.app_locale',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'en-US',
      },
      {
        key: 'intl.accept_languages',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'en-US,sw',
      },
      {
        key: 'spellcheck.dictionaries',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: ['en-US'],
      },
      {
        key: 'translate_blocked_languages',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: ['en-US'],
      }
    ];
    if (isChromeOS) {
      fakePrefs.push({
        key: 'settings.language.preferred_languages',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'en-US,sw',
      });
      fakePrefs.push({
        key: 'settings.language.preload_engines',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng,' +
            '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
      });
      fakePrefs.push({
        key: 'settings.language.enabled_extension_imes',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '',
      });
    }
    return fakePrefs;
  }

  /** @type {?SettingsEditDictionaryPageElement} */
  let editDictPage;
  /** @type {?FakeLanguageSettingsPrivate} */
  let languageSettingsPrivate;
  /** @type {?FakeSettingsPrivate} */
  let settingsPrefs;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    PolymerTest.clearBody();
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    languageSettingsPrivate = new FakeLanguageSettingsPrivate();
    languageSettingsPrivate.setSettingsPrefs(settingsPrefs);
    const browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.instance_ = browserProxy;
    browserProxy.setLanguageSettingsPrivate(languageSettingsPrivate);

    editDictPage = document.createElement('settings-edit-dictionary-page');

    // Prefs would normally be data-bound to settings-languages.
    document.body.appendChild(editDictPage);
  });

  teardown(function() {
    editDictPage.remove();
  });

  test('add word validation', function() {
    // Check addWord enable/disable logic
    const addWordButton = editDictPage.$.addWord;
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = '';
    assertTrue(addWordButton.disabled);
    editDictPage.$.newWord.value = 'valid word';
    assertFalse(addWordButton.disabled);
    assertFalse(
        window.getComputedStyle(addWordButton)['pointer-events'] ===
        'none');  // Make sure add-word button actually clickable.
  });

  test('add duplicate word', function() {
    const WORD = 'unique';
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([WORD], []);
    editDictPage.$.newWord.value = `${WORD} ${WORD}`;
    flush();
    assertFalse(editDictPage.$.addWord.disabled);

    editDictPage.$.newWord.value = WORD;
    flush();
    assertTrue(editDictPage.$.addWord.disabled);

    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([], [WORD]);
    flush();
    assertFalse(editDictPage.$.addWord.disabled);
  });

  test('spellcheck edit dictionary page message when empty', function() {
    assertTrue(!!editDictPage);
    return languageSettingsPrivate.whenCalled('getSpellcheckWords')
        .then(function() {
          flush();

          assertFalse(editDictPage.$.noWordsLabel.hidden);
          assertFalse(!!editDictPage.$$('#list'));
        });
  });

  test('spellcheck edit dictionary page list has words', function() {
    const addWordButton = editDictPage.$$('#addWord');
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(2, editDictPage.$$('#list').items.length);
  });

  test('spellcheck edit dictionary page remove is in tab order', function() {
    const addWordButton = editDictPage.$$('#addWord');
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(1, editDictPage.$$('#list').items.length);

    const removeWordButton = editDictPage.$$('cr-icon-button');
    // Button should be reachable in the tab order.
    assertEquals('0', removeWordButton.getAttribute('tabindex'));
    removeWordButton.click();
    flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);

    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(1, editDictPage.$$('#list').items.length);
    const newRemoveWordButton = editDictPage.$$('cr-icon-button');
    // Button should be reachable in the tab order.
    assertEquals('0', newRemoveWordButton.getAttribute('tabindex'));
  });
});
