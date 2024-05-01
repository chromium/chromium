// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {LanguagesBrowserProxyImpl, OsSettingsEditDictionaryPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, IronListElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';
import {clearBody} from '../utils.js';

import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.js';

suite('<os-settings-edit-dictionary-page>', () => {
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

  let editDictPage: OsSettingsEditDictionaryPageElement;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;
  let settingsPrefs: SettingsPrefsElement;
  let browserProxy: TestLanguagesBrowserProxy;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    clearBody();
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    languageSettingsPrivate = new FakeLanguageSettingsPrivate();
    languageSettingsPrivate.setSettingsPrefsForTesting(settingsPrefs);
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstanceForTesting(browserProxy);
    browserProxy.setLanguageSettingsPrivate(
        languageSettingsPrivate as unknown as
        typeof chrome.languageSettingsPrivate);

    editDictPage = document.createElement('os-settings-edit-dictionary-page');

    // Prefs would normally be data-bound to settings-languages.
    document.body.appendChild(editDictPage);
  });

  teardown(() => {
    editDictPage.remove();
    settingsPrefs.remove();
    languageSettingsPrivate.reset();
    browserProxy.reset();
  });

  test('adds word validation', () => {
    // Check addWord enable/disable logic
    const addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = '';
    assertTrue(addWordButton.disabled);
    editDictPage.$.newWord.value = 'valid word';
    assertFalse(addWordButton.disabled);
    const computedStyle = window.getComputedStyle(addWordButton);
    const events = computedStyle.getPropertyValue('pointer-events');
    assertNotEquals(
        'none', events);  // Make sure add-word button is actually clickable.
  });

  test('shows error when adding duplicate word', () => {
    const WORD = 'unique';
    loadTimeData.overrideValues({
      addDictionaryWordDuplicateError: 'duplicate',
    });
    // add word
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([WORD], []);
    editDictPage.$.newWord.value = `${WORD} ${WORD}`;
    flush();
    let addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertFalse(addWordButton.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals('', editDictPage.$.newWord.errorMessage);

    // add duplicate word
    editDictPage.$.newWord.value = WORD;
    flush();
    addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertTrue(addWordButton.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals('duplicate', editDictPage.$.newWord.errorMessage);

    // remove word
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([], [WORD]);
    flush();
    addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertFalse(addWordButton.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals('', editDictPage.$.newWord.errorMessage);
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
    flush();
    let addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertFalse(addWordButton.disabled);
    assertFalse(editDictPage.$.newWord.invalid);
    assertEquals('', editDictPage.$.newWord.errorMessage);

    editDictPage.$.newWord.value = TOO_LONG_WORD;
    flush();
    addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertTrue(addWordButton.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals('too long', editDictPage.$.newWord.errorMessage);

    editDictPage.$.newWord.value = TOO_BIG_WORD;
    flush();

    addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    assertTrue(addWordButton.disabled);
    assertTrue(editDictPage.$.newWord.invalid);
    assertEquals('too long', editDictPage.$.newWord.errorMessage);
  });

  test('shows message when empty', async () => {
    assertTrue(!!editDictPage);
    await languageSettingsPrivate.whenCalled('getSpellcheckWords');
    flush();
    const noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertFalse(noWordsLabel.hidden);
  });

  test('adds words', () => {
    const addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    flush();

    const noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertTrue(noWordsLabel.hidden);
    const list =
        editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    const listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(2, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('valid word2', listItems[0]);
    assertEquals('valid word', listItems[1]);
  });

  test('removes word', () => {
    const addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    flush();

    let list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    let listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(1, listItems.length);

    const removeWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!removeWordButton);
    removeWordButton.click();
    flush();

    const noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertFalse(noWordsLabel.hidden);
    list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(0, listItems.length);
  });

  test('syncs removed and added words', () => {
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners(
        /*added=*/['word1', 'word2', 'word3'], /*removed=*/[]);
    flush();

    let list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    let listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(3, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('word3', listItems[0]);
    assertEquals('word2', listItems[1]);
    assertEquals('word1', listItems[2]);

    languageSettingsPrivate.onCustomDictionaryChanged.callListeners(
        /*added=*/['word4'], /*removed=*/['word2', 'word3']);
    flush();

    list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(2, listItems.length);
    // list is shown with latest word added on top.
    assertEquals('word4', listItems[0]);
    assertEquals('word1', listItems[1]);
  });

  test('removes is in tab order', () => {
    const addWordButton =
        editDictPage.shadowRoot!.querySelector<HTMLButtonElement>('#addWord');
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = 'valid word';
    addWordButton.click();
    flush();

    let noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertTrue(noWordsLabel.hidden);
    let list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    let listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(1, listItems.length);

    const removeWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!removeWordButton);
    // Button should be reachable in the tab order.
    assertEquals('0', removeWordButton.getAttribute('tabindex'));
    removeWordButton.click();
    flush();

    noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertFalse(noWordsLabel.hidden);

    editDictPage.$.newWord.value = 'valid word2';
    addWordButton.click();
    flush();

    noWordsLabel =
        editDictPage.shadowRoot!.querySelector<HTMLElement>('#noWordsLabel');
    assertTrue(!!noWordsLabel);
    assertTrue(noWordsLabel.hidden);
    list = editDictPage.shadowRoot!.querySelector<IronListElement>('#list');
    assertTrue(!!list);
    listItems = list.items;
    assertTrue(!!listItems);
    assertEquals(1, listItems.length);
    const newRemoveWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!newRemoveWordButton);
    // Button should be reachable in the tab order.
    assertEquals('0', newRemoveWordButton.getAttribute('tabindex'));
  });
});
