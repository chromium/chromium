// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsEditDictionaryPageElement} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('EditDictionaryPage', function() {
  let editDictPage: SettingsEditDictionaryPageElement;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    languageSettingsPrivate = new FakeLanguageSettingsPrivate();
    const browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setLanguageSettingsPrivate(
        languageSettingsPrivate as unknown as
        typeof chrome.languageSettingsPrivate);

    editDictPage = document.createElement('settings-edit-dictionary-page');
    document.body.appendChild(editDictPage);
    return languageSettingsPrivate.whenCalled('getSpellcheckWords');
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

  test('Enter/Escape key event', async () => {
    // Add a new word by pressing Enter.
    const WORD = 'testEnter';
    editDictPage.$.newWord.value = WORD;
    await microtasksFinished();
    keyDownOn(editDictPage.$.newWord, 0, [], 'Enter');
    assertEquals(
        WORD, await languageSettingsPrivate.whenCalled('addSpellcheckWord'));

    flush();

    // Clear input by pressing Escape.
    editDictPage.$.newWord.value = 'testEscape';
    await microtasksFinished();
    keyDownOn(editDictPage.$.newWord, 0, [], 'Escape');
    await microtasksFinished();
    assertEquals('', editDictPage.$.newWord.value);
  });

  test('spellcheck edit dictionary page message when empty', async function() {
    assertTrue(!!editDictPage);
    await languageSettingsPrivate.whenCalled('getSpellcheckWords');

    flush();

    assertFalse(editDictPage.$.noWordsLabel.hidden);
    assertFalse(!!editDictPage.shadowRoot!.querySelector('#list'));
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
    assertTrue(!!editDictPage.shadowRoot!.querySelector('#list'));
    assertEquals(2, editDictPage.shadowRoot!.querySelectorAll('.word').length);
  });

  test('spellcheck edit dictionary page remove is in tab order', async () => {
    const addWordButton = editDictPage.$.addWord;
    editDictPage.$.newWord.value = 'valid word';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    assertTrue(editDictPage.$.noWordsLabel.hidden);
    assertTrue(!!editDictPage.shadowRoot!.querySelector('#list'));
    assertEquals(1, editDictPage.shadowRoot!.querySelectorAll('.word').length);

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
    assertTrue(!!editDictPage.shadowRoot!.querySelector('#list'));
    assertEquals(1, editDictPage.shadowRoot!.querySelectorAll('.word').length);
    const newRemoveWordButton =
        editDictPage.shadowRoot!.querySelector('cr-icon-button')!;
    // Button should be reachable in the tab order.
    assertEquals('0', newRemoveWordButton.getAttribute('tabindex'));
  });

  test('EditDictionaryPageFocusgroup', async () => {
    const addWordButton = editDictPage.$.addWord;
    editDictPage.$.newWord.value = 'valid word';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    const list = editDictPage.shadowRoot!.querySelector('#list')!;
    assertEquals('listbox block', list.getAttribute('focusgroup'));
  });
});

suite('EditDictionaryPageFocus', function() {
  let editDictPage: SettingsEditDictionaryPageElement;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    languageSettingsPrivate = new FakeLanguageSettingsPrivate();
    const browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setLanguageSettingsPrivate(
        languageSettingsPrivate as unknown as
        typeof chrome.languageSettingsPrivate);

    editDictPage = document.createElement('settings-edit-dictionary-page');
    document.body.appendChild(editDictPage);
    return languageSettingsPrivate.whenCalled('getSpellcheckWords');
  });

  test('focus restored when last item deleted', async () => {
    const addWordButton = editDictPage.$.addWord;
    editDictPage.$.newWord.value = 'word1';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    editDictPage.$.newWord.value = 'word2';
    await editDictPage.$.newWord.updateComplete;
    addWordButton.click();
    flush();

    const buttons = editDictPage.shadowRoot!.querySelectorAll('cr-icon-button');
    assertEquals(2, buttons.length);

    // Focus the second item's delete button and click it.
    buttons[1]!.focus();
    buttons[1]!.click();
    flush();

    // Focus should be restored to the remaining item's delete button.
    assertEquals(buttons[0], editDictPage.shadowRoot!.activeElement);
  });
});
