// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AccentMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {NotificationType, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createSpeechSynthesisVoice, setupTestEnvironment} from './common.js';

suite('AccentMenu', () => {
  let accentMenu: AccentMenuElement;
  let availableVoices: SpeechSynthesisVoice[];

  function getAccentButtons(): NodeListOf<HTMLButtonElement> {
    return accentMenu.$.accentMenu.querySelectorAll<HTMLButtonElement>(
        '.dropdown-item');
  }

  function getSearchField() {
    return accentMenu.$.searchField;
  }

  setup(async () => {
    setupTestEnvironment();
    accentMenu = document.createElement('accent-menu');
    accentMenu.localesOfLangPackVoices = new Set(['en-us', 'en-gb']);
    document.body.appendChild(accentMenu);

    availableVoices = [
      createSpeechSynthesisVoice({name: 'Voice 1', lang: 'en-US'}),
      createSpeechSynthesisVoice({name: 'Voice 2', lang: 'en-GB'}),
    ];
    accentMenu.availableVoices = availableVoices;
    accentMenu.localeToDisplayName = {
      'en-us': 'English (United States)',
      'en-gb': 'English (United Kingdom)',
    };
    accentMenu.selectedLang = 'en-us';
    await microtasksFinished();
  });

  test('renders radio accessibility semantics and active checkmark', () => {
    const body =
        accentMenu.$.accentMenu.querySelector<HTMLElement>('.accent-menu-body');
    assertTrue(!!body);
    assertEquals('radiogroup', body.getAttribute('role'));

    const buttons = getAccentButtons();
    assertTrue(buttons.length >= 2);

    buttons.forEach(button => {
      assertEquals('radio', button.getAttribute('role'));
    });

    // en-gb is index 0 (alphabetical by readable language), en-us is index 1
    const enGbButton = buttons[0]!;
    const enUsButton = buttons[1]!;

    assertEquals('false', enGbButton.getAttribute('aria-checked'));
    assertEquals('true', enUsButton.getAttribute('aria-checked'));

    const activeCheckMarks =
        accentMenu.$.accentMenu.querySelectorAll<HTMLElement>(
            '.check-mark-showing-true');
    assertEquals(1, activeCheckMarks.length);
  });

  test('fires LANGUAGE_SELECTED event on unselected accent click', async () => {
    const whenFired = eventToPromise<CustomEvent<{language: string}>>(
        ToolbarEvent.LANGUAGE_SELECTED, accentMenu);
    const buttons = getAccentButtons();
    const enGbButton = buttons[0]!;
    enGbButton.click();
    const event = await whenFired;
    assertEquals('en-gb', event.detail.language);
  });

  test('retains checkmark on active accent during download', async () => {
    accentMenu.notify(NotificationType.DOWNLOADING, 'en-gb');
    await microtasksFinished();

    const buttons = getAccentButtons();
    const enGbButton = buttons[0]!;
    const enUsButton = buttons[1]!;

    const enUsCheckMark =
        enUsButton.querySelector<HTMLElement>('.check-mark-showing-true');
    assertTrue(!!enUsCheckMark);

    const enGbCheckMark =
        enGbButton.querySelector<HTMLElement>('.check-mark-showing-false');
    assertTrue(!!enGbCheckMark);

    const notification = accentMenu.$.accentMenu.querySelector<HTMLElement>(
        '#notificationText-0');
    assertTrue(!!notification);
    assertFalse(notification.classList.contains('notification-error-true'));
    assertTrue(notification.classList.contains('notification-text'));
  });

  test('displays error notification without shifting checkmark', async () => {
    accentMenu.notify(NotificationType.GENERIC_ERROR, 'en-gb');
    await microtasksFinished();

    const buttons = getAccentButtons();
    const enUsButton = buttons[1]!;

    const enUsCheckMark =
        enUsButton.querySelector<HTMLElement>('.check-mark-showing-true');
    assertTrue(!!enUsCheckMark);

    const notification = accentMenu.$.accentMenu.querySelector<HTMLElement>(
        '#notificationText-0');
    assertTrue(!!notification);
    assertTrue(notification.classList.contains('notification-error-true'));
    assertTrue(notification.classList.contains('notification-text'));
  });

  test(
      'maintains accurate click index binding during search filtering',
      async () => {
        accentMenu.availableVoices = [
          ...availableVoices,
          createSpeechSynthesisVoice({name: 'Voice 3', lang: 'es-ES'}),
        ];
        accentMenu.localesOfLangPackVoices =
            new Set(['en-us', 'en-gb', 'es-es']);
        accentMenu.localeToDisplayName = {
          ...accentMenu.localeToDisplayName,
          'es-es': 'Spanish (Spain)',
        };
        await microtasksFinished();

        let selectedLanguage = '';
        accentMenu.addEventListener(
            ToolbarEvent.LANGUAGE_SELECTED, (e: Event) => {
              selectedLanguage =
                  (e as CustomEvent<{language: string}>).detail.language;
            });

        getSearchField().value = 'United Kingdom';
        await microtasksFinished();

        const filteredButtons = getAccentButtons();
        assertEquals(1, filteredButtons.length);

        filteredButtons[0]!.click();
        await microtasksFinished();

        assertEquals('en-gb', selectedLanguage);
      });

  test('clears search on clear button click', async () => {
    getSearchField().value = 'United Kingdom';
    await microtasksFinished();
    assertEquals(1, getAccentButtons().length);

    const clearButton = accentMenu.$.accentMenu.querySelector<HTMLElement>(
        '#clearLanguageSearch');
    assertTrue(!!clearButton);
    clearButton.click();
    await microtasksFinished();

    assertEquals('', getSearchField().value);
    assertTrue(getAccentButtons().length >= 2);
  });
});
