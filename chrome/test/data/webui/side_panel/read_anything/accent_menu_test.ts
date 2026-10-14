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

  test('Tab on last accent item wraps focus to close button', () => {
    const buttons = getAccentButtons();
    const lastButton = buttons[buttons.length - 1]!;
    const closeButton =
        accentMenu.$.accentMenu.shadowRoot.querySelector<HTMLElement>('#close')!
        ;

    lastButton.focus();
    assertEquals(lastButton, accentMenu.shadowRoot.activeElement);

    const tabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    lastButton.dispatchEvent(tabEvent);

    assertTrue(tabEvent.defaultPrevented);
    assertEquals(accentMenu.$.accentMenu, accentMenu.shadowRoot.activeElement);
    assertEquals(closeButton, accentMenu.$.accentMenu.shadowRoot.activeElement);
  });

  test('Shift+Tab on close button wraps focus to last accent item', () => {
    const buttons = getAccentButtons();
    const lastButton = buttons[buttons.length - 1]!;
    const closeButton =
        accentMenu.$.accentMenu.shadowRoot.querySelector<HTMLElement>('#close')!
        ;

    closeButton.focus();
    assertEquals(accentMenu.$.accentMenu, accentMenu.shadowRoot.activeElement);
    assertEquals(closeButton, accentMenu.$.accentMenu.shadowRoot.activeElement);

    const shiftTabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    closeButton.dispatchEvent(shiftTabEvent);

    assertTrue(shiftTabEvent.defaultPrevented);
    assertEquals(lastButton, accentMenu.shadowRoot.activeElement);
  });

  test('sets roving tabindex on selected accent or first visible item',
      async () => {
        const buttons = getAccentButtons();
        assertEquals(2, buttons.length);
        // en-gb is index 0 (unselected), en-us is index 1 (selected).
        assertEquals(-1, buttons[0]!.tabIndex);
        assertEquals(0, buttons[1]!.tabIndex);

        // When search filters out the selected accent, fallback to index 0.
        getSearchField().value = 'United Kingdom';
        await microtasksFinished();

        const filteredButtons = getAccentButtons();
        assertEquals(1, filteredButtons.length);
        assertEquals(0, filteredButtons[0]!.tabIndex);
      });

  test('arrow keys navigate accent items and update roving tabindex', () => {
    const buttons = getAccentButtons();
    const enGbButton = buttons[0]!;
    const enUsButton = buttons[1]!;

    enUsButton.focus();
    assertEquals(enUsButton, accentMenu.shadowRoot.activeElement);

    const upEvent = new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    enUsButton.dispatchEvent(upEvent);

    assertTrue(upEvent.defaultPrevented);
    assertEquals(enGbButton, accentMenu.shadowRoot.activeElement);
    assertEquals(0, enGbButton.tabIndex);
    assertEquals(-1, enUsButton.tabIndex);

    const downEvent = new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    enGbButton.dispatchEvent(downEvent);

    assertTrue(downEvent.defaultPrevented);
    assertEquals(enUsButton, accentMenu.shadowRoot.activeElement);
    assertEquals(-1, enGbButton.tabIndex);
    assertEquals(0, enUsButton.tabIndex);
  });

  test('arrow keys wrap around at first and last accent items', () => {
    const buttons = getAccentButtons();
    const enGbButton = buttons[0]!;
    const enUsButton = buttons[1]!;

    enGbButton.focus();
    assertEquals(enGbButton, accentMenu.shadowRoot.activeElement);

    // ArrowUp on first item wraps around to last item.
    const wrapUpEvent = new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    enGbButton.dispatchEvent(wrapUpEvent);

    assertTrue(wrapUpEvent.defaultPrevented);
    assertEquals(enUsButton, accentMenu.shadowRoot.activeElement);
    assertEquals(-1, enGbButton.tabIndex);
    assertEquals(0, enUsButton.tabIndex);

    // ArrowDown on last item wraps around to first item.
    const wrapDownEvent = new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    enUsButton.dispatchEvent(wrapDownEvent);

    assertTrue(wrapDownEvent.defaultPrevented);
    assertEquals(enGbButton, accentMenu.shadowRoot.activeElement);
    assertEquals(0, enGbButton.tabIndex);
    assertEquals(-1, enUsButton.tabIndex);
  });

  test('arrow keys on search field are not intercepted', () => {
    const searchField = getSearchField();
    searchField.focus();
    assertEquals(searchField, accentMenu.shadowRoot.activeElement);

    const searchArrowEvent = new KeyboardEvent('keydown', {
      key: 'ArrowLeft',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    searchField.dispatchEvent(searchArrowEvent);

    assertFalse(searchArrowEvent.defaultPrevented);
    assertEquals(searchField, accentMenu.shadowRoot.activeElement);
  });

  test('Tab and Shift+Tab wrap to active roving tabindex item', async () => {
    accentMenu.selectedLang = 'en-gb';
    await microtasksFinished();

    const buttons = getAccentButtons();
    const enGbButton = buttons[0]!;
    const enUsButton = buttons[1]!;
    const closeButton =
        accentMenu.$.accentMenu.shadowRoot.querySelector<HTMLElement>('#close')!
        ;

    assertEquals(0, enGbButton.tabIndex);
    assertEquals(-1, enUsButton.tabIndex);

    enGbButton.focus();
    assertEquals(enGbButton, accentMenu.shadowRoot.activeElement);

    const tabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    enGbButton.dispatchEvent(tabEvent);

    assertTrue(tabEvent.defaultPrevented);
    assertEquals(accentMenu.$.accentMenu, accentMenu.shadowRoot.activeElement);
    assertEquals(closeButton, accentMenu.$.accentMenu.shadowRoot.activeElement);

    const shiftTabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    closeButton.dispatchEvent(shiftTabEvent);

    assertTrue(shiftTabEvent.defaultPrevented);
    assertEquals(enGbButton, accentMenu.shadowRoot.activeElement);
  });
});
