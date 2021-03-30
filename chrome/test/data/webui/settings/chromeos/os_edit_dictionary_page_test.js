// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';
// #import {FakeSettingsPrivate} from '../fake_settings_private.js';
// #import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.m.js';
// clang-format on

suite('edit dictionary page', () => {
  function getFakePrefs() {
    return [
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
      },
      {
        key: 'settings.language.preferred_languages',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'en-US,sw',
      },
      {
        key: 'settings.language.preload_engines',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng,' +
            '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
      },
      {
        key: 'settings.language.enabled_extension_imes',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '',
      },
    ];
  }

  /** @type {?settings.SettingsEditDictionaryPageElement} */
  let editDictPage;
  /** @type {?settings.FakeLanguageSettingsPrivate} */
  let languageSettingsPrivate;
  /** @type {?settings.FakeSettingsPrivate} */
  let settingsPrefs;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
    loadTimeData.overrideValues({enableLanguageSettingsV2: true});
  });

  setup(() => {
    document.body.innerHTML = '';
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new settings.FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    languageSettingsPrivate = new settings.FakeLanguageSettingsPrivate();
    languageSettingsPrivate.setSettingsPrefs(settingsPrefs);
    const browserProxy = new settings.TestLanguagesBrowserProxy();
    settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;
    browserProxy.setLanguageSettingsPrivate(languageSettingsPrivate);

    editDictPage = document.createElement('os-settings-edit-dictionary-page');

    // Prefs would normally be data-bound to settings-languages.
    document.body.appendChild(editDictPage);
  });

  test('adds word validation', () => {
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

  test('shows error when adding duplicate word', () => {
    const WORD = 'unique';
    loadTimeData.overrideValues({
      addDictionaryWordDuplicateError: 'duplicate',
    });
    // add word
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([WORD], []);
    editDictPage.$.newWord.value = `${WORD} ${WORD}`;
    Polymer.dom.flush();
    assertFalse(editDictPage.$.addWord.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, '');

    // add duplicate word
    editDictPage.$.newWord.value = WORD;
    Polymer.dom.flush();
    assertTrue(editDictPage.$.addWord.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, 'duplicate');

    // remove word
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([], [WORD]);
    Polymer.dom.flush();
    assertFalse(editDictPage.$.addWord.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, '');
  });

  test('shows error when adding word bigger than 99 bytes', () => {
    const OK_WORD = 'u'.repeat(99);
    const TOO_LONG_WORD = 'u'.repeat(100);
    // This emoji has length 2 and bytesize 4.
    const TOO_BIG_WORD = '😎'.repeat(25);
    loadTimeData.overrideValues({
      addDictionaryWordLengthError: 'too long',
    });

    editDictPage.$.newWord.value = OK_WORD;
    Polymer.dom.flush();

    assertFalse(editDictPage.$.addWord.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, '');

    editDictPage.$.newWord.value = TOO_LONG_WORD;
    Polymer.dom.flush();

    assertTrue(editDictPage.$.addWord.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, 'too long');

    editDictPage.$.newWord.value = TOO_BIG_WORD;
    Polymer.dom.flush();

    assertTrue(editDictPage.$.addWord.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals(editDictPage.$.newWord.errorMessage, 'too long');
  });

  test('shows message when empty', () => {
    assertTrue(!!editDictPage);
    return languageSettingsPrivate.whenCalled('getSpellcheckWords').then(() => {
      Polymer.dom.flush();

      assertFalse(editDictPage.$.noWordsLabel.hidden);
    });
  });

  test('adds words', () => {
    const addWordButton = editDictPage.$$('#addWord');
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    Polymer.dom.flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    const listItems = editDictPage.$$('#list').items;
    assertEquals(2, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('valid word2', listItems[0]);
    assertEquals('valid word', listItems[1]);
  });

  test('removes word', () => {
    const addWordButton = editDictPage.$$('#addWord');
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    Polymer.dom.flush();

    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(1, editDictPage.$$('#list').items.length);

    const removeWordButton = editDictPage.$$('cr-icon-button');
    removeWordButton.click();
    Polymer.dom.flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(0, editDictPage.$$('#list').items.length);
  });

  test('syncs removed and added words', () => {
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners(
        /*added=*/['word1', 'word2', 'word3'], /*removed=*/[]);
    Polymer.dom.flush();

    assertTrue(!!editDictPage.$$('#list'));
    let listItems = editDictPage.$$('#list').items;
    assertEquals(3, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('word3', listItems[0]);
    assertEquals('word2', listItems[1]);
    assertEquals('word1', listItems[2]);

    languageSettingsPrivate.onCustomDictionaryChanged.callListeners(
        /*added=*/['word4'], /*removed=*/['word2', 'word3']);
    Polymer.dom.flush();

    assertTrue(!!editDictPage.$$('#list'));
    listItems = editDictPage.$$('#list').items;
    assertEquals(2, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('word4', listItems[0]);
    assertEquals('word1', listItems[1]);
  });

  test('removes is in tab order', () => {
    const addWordButton = editDictPage.$$('#addWord');
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    Polymer.dom.flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(1, editDictPage.$$('#list').items.length);

    const removeWordButton = editDictPage.$$('cr-icon-button');
    // Button should be reachable in the tab order.
    assertEquals('0', removeWordButton.getAttribute('tabindex'));
    removeWordButton.click();
    Polymer.dom.flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);

    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    Polymer.dom.flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.$$('#list'));
    assertEquals(1, editDictPage.$$('#list').items.length);
    const newRemoveWordButton = editDictPage.$$('cr-icon-button');
    // Button should be reachable in the tab order.
    assertEquals('0', newRemoveWordButton.getAttribute('tabindex'));
  });
});
