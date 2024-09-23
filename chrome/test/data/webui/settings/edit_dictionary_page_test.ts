// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsEditDictionaryPageElement} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

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
      },
    ];
    return fakePrefs;
  }

  let editDictPage: SettingsEditDictionaryPageElement;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    languageSettingsPrivate = new FakeLanguageSettingsPrivate();
    languageSettingsPrivate.setSettingsPrefs(settingsPrefs);
    const browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setLanguageSettingsPrivate(
        languageSettingsPrivate as unknown as
        typeof chrome.languageSettingsPrivate);

    editDictPage = document.createElement('settings-edit-dictionary-page');

    // Prefs would normally be data-bound to settings-languages.
    document.body.appendChild(editDictPage);
    return languageSettingsPrivate.whenCalled('getSpellcheckWords');
  });

  teardown(function() {
    editDictPage.remove();
  });

  test('add word validation', async () => {
    // Check addWord enable/disable logic
    const addWordButton = editDictPage.$.addWord;
    assertTrue(!!addWordButton);
    editDictPage.$.newWord.value = '';
    await editDictPage.$.newWord.updateComplete;
    assertTrue(addWordButton.disabled);
    editDictPage.$.newWord.value = 'valid word';
    await editDictPage.$.newWord.updateComplete;
    assertFalse(addWordButton.disabled);
    assertFalse(
        window.getComputedStyle(addWordButton)
            .getPropertyValue('pointer-events') ===
        'none');  // Make sure add-word button actually clickable.
  });

  test('add duplicate word', async () => {
    const WORD = 'unique';
    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([WORD], []);
    editDictPage.$.newWord.value = `${WORD} ${WORD}`;
    await editDictPage.$.newWord.updateComplete;
    flush();
    assertFalse(editDictPage.$.addWord.disabled);

    editDictPage.$.newWord.value = WORD;
    await editDictPage.$.newWord.updateComplete;
    flush();
    assertTrue(editDictPage.$.addWord.disabled);

    languageSettingsPrivate.onCustomDictionaryChanged.callListeners([], [WORD]);
    flush();
    assertFalse(editDictPage.$.addWord.disabled);
  });

  test('spellcheck edit dictionary page message when empty', async function() {
    assertTrue(!!editDictPage);
    await languageSettingsPrivate.whenCalled('getSpellcheckWords');

    flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);
    assertFalse(!!editDictPage.shadowRoot!.querySelector('iron-list'));
  });

  test('spellcheck edit dictionary page list has words', async () => {
    const addWordButton = editDictPage.$.addWord;
    editDictPage.$.newWord.value = 'valid word';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    editDictPage.$.newWord.value = 'valid word2';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.shadowRoot!.querySelector('iron-list'));
    assertEquals(
        2, editDictPage.shadowRoot!.querySelector('iron-list')!.items!.length);
  });

  test('spellcheck edit dictionary page remove is in tab order', async () => {
    const addWordButton = editDictPage.$.addWord;
    editDictPage.$.newWord.value = 'valid word';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.shadowRoot!.querySelector('iron-list'));
    assertEquals(
        1, editDictPage.shadowRoot!.querySelector('iron-list')!.items!.length);

    const removeWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button')!;
    // Button should be reachable in the tab order.
    assertEquals('0', removeWordButton.getAttribute('tabindex'));
    removeWordButton.click();
    flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);

    editDictPage.$.newWord.value = 'valid word2';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.shadowRoot!.querySelector('iron-list'));
    assertEquals(
        1, editDictPage.shadowRoot!.querySelector('iron-list')!.items!.length);
    const newRemoveWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button')!;
    // Button should be reachable in the tab order.
    assertEquals('0', newRemoveWordButton.getAttribute('tabindex'));
  });
});
