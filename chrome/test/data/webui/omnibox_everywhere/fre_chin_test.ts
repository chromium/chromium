// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/fre_chin.js';

import {FreChinMode} from 'chrome://omnibox-everywhere.top-chrome/fre_chin.js';
import type {FreChinElement, ShowHotkeyDropdownDetail} from 'chrome://omnibox-everywhere.top-chrome/fre_chin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('FreChinTest', () => {
  let freChin: FreChinElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      loomniboxFreSelectKeyboardShortcut: 'Select keyboard shortcut',
      loomniboxFreSelectShortcut: 'Select shortcut',
      loomniboxFreToSearchOrCustomize:
          'to search with Chrome or customize in ' +
          '<a href="#" class="settings-link">Settings</a>',
      loomniboxFreReminderToSearch: 'to search with Chrome.',
      loomniboxFreChangeShortcutIn:
          'Change shortcut in <a href="#" class="settings-link">Settings</a>',
      loomniboxFreCloseButtonAria: 'Close',
    });
    freChin = document.createElement('fre-chin');
    freChin.hotkeyTokens = ['Cmd', 'Shift', 'Space'];
    freChin.mode = FreChinMode.SHORTCUT_SETUP;
    document.body.appendChild(freChin);
    await freChin.updateComplete;
  });

  test('renders shortcut setup mode correctly with hotkey tokens', () => {
    assertTrue(isVisible(freChin));
    const dropdownTrigger = freChin.shadowRoot.querySelector<HTMLButtonElement>(
        '.dropdown-trigger');
    assertTrue(!!dropdownTrigger);
    assertEquals(
        'Select keyboard shortcut: Cmd Shift Space',
        dropdownTrigger.getAttribute('aria-label'));
    assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));

    const keyBadges =
        freChin.shadowRoot.querySelectorAll<HTMLElement>('.key-badge');
    assertEquals(3, keyBadges.length);
    assertEquals('Cmd', keyBadges[0]!.textContent.trim());
    assertEquals('Shift', keyBadges[1]!.textContent.trim());
    assertEquals('Space', keyBadges[2]!.textContent.trim());

    const caret = freChin.shadowRoot.querySelector('.dropdown-caret');
    assertTrue(!!caret);

    const settingsLink =
        freChin.shadowRoot.querySelector<HTMLAnchorElement>('.settings-link');
    assertTrue(!!settingsLink);
    assertEquals('Settings', settingsLink.textContent.trim());

    const label =
        freChin.shadowRoot.querySelector<HTMLElement>('.to-search-label');
    assertTrue(!!label);
    assertTrue(
        label.textContent.includes('to search with Chrome or customize in'));
  });

  test(
      'renders select shortcut placeholder when hotkey tokens are empty',
      async () => {
        freChin.hotkeyTokens = [];
        await freChin.updateComplete;

        const dropdownTrigger =
            freChin.shadowRoot.querySelector<HTMLButtonElement>(
                '.dropdown-trigger');
        assertTrue(!!dropdownTrigger);
        assertEquals(
            'Select keyboard shortcut',
            dropdownTrigger.getAttribute('aria-label'));
        assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));

        const selectLabel = freChin.shadowRoot.querySelector<HTMLElement>(
            '.select-shortcut-label');
        assertTrue(!!selectLabel);
        assertEquals('Select shortcut', selectLabel.textContent.trim());

        const keyBadges =
            freChin.shadowRoot.querySelectorAll<HTMLElement>('.key-badge');
        assertEquals(0, keyBadges.length);
      });

  test(
      'clicking dropdown trigger fires show-hotkey-dropdown event with bounds',
      async () => {
        const dropdownTrigger =
            freChin.shadowRoot.querySelector<HTMLButtonElement>(
                '.dropdown-trigger')!;
        const showDropdownPromise =
            eventToPromise<CustomEvent<ShowHotkeyDropdownDetail>>(
                'show-hotkey-dropdown', freChin);
        dropdownTrigger.click();
        const event = await showDropdownPromise;
        assertTrue(!!event.detail);
        assertEquals('number', typeof event.detail.x);
        assertEquals('number', typeof event.detail.y);
        assertEquals('number', typeof event.detail.width);
        assertEquals('number', typeof event.detail.height);
      });

  test(
      'dropdown trigger aria-expanded reflects dropdownOpen property',
      async () => {
        const dropdownTrigger =
            freChin.shadowRoot.querySelector<HTMLButtonElement>(
                '.dropdown-trigger');
        assertTrue(!!dropdownTrigger);
        assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));

        freChin.dropdownOpen = true;
        await freChin.updateComplete;
        assertEquals('true', dropdownTrigger.getAttribute('aria-expanded'));

        freChin.dropdownOpen = false;
        await freChin.updateComplete;
        assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));
      });

  test('clicking settings link fires open-settings event', async () => {
    const settingsLink =
        freChin.shadowRoot.querySelector<HTMLAnchorElement>('.settings-link')!;
    const settingsPromise = eventToPromise('open-settings', freChin);
    settingsLink.click();
    await settingsPromise;
  });

  test('clicking inside settings link fires open-settings event', async () => {
    const settingsLink =
        freChin.shadowRoot.querySelector<HTMLAnchorElement>('.settings-link')!;
    const innerSpan = document.createElement('span');
    innerSpan.textContent = 'Settings';
    settingsLink.textContent = '';
    settingsLink.appendChild(innerSpan);

    const settingsPromise = eventToPromise('open-settings', freChin);
    innerSpan.click();
    await settingsPromise;
  });

  test('renders shortcut reminder mode correctly', async () => {
    freChin.mode = FreChinMode.SHORTCUT_REMINDER;
    await freChin.updateComplete;

    // Dropdown trigger should not be present in reminder mode.
    const dropdownTrigger =
        freChin.shadowRoot.querySelector('.dropdown-trigger');
    assertEquals(null, dropdownTrigger);

    // White hotkey container, key badges, and reminder settings link
    // should be present.
    const hotkeyContainer =
        freChin.shadowRoot.querySelector('.hotkey-container');
    assertTrue(!!hotkeyContainer);

    const keyBadges =
        freChin.shadowRoot.querySelectorAll<HTMLElement>('.key-badge');
    assertEquals(3, keyBadges.length);

    const changeShortcutLabel =
        freChin.shadowRoot.querySelector('.change-shortcut-label');
    assertTrue(!!changeShortcutLabel);
    assertEquals(
        'Change shortcut in Settings',
        changeShortcutLabel.textContent.replace(/\s+/g, ' ').trim());

    const settingsLink =
        freChin.shadowRoot.querySelector<HTMLAnchorElement>('.settings-link');
    assertTrue(!!settingsLink);
    assertEquals('Settings', settingsLink.textContent.trim());
  });

  test('clicking close button fires close event', async () => {
    const closeBtn =
        freChin.shadowRoot.querySelector<HTMLElement>('.close-button')!;
    const closePromise = eventToPromise('close', freChin);
    closeBtn.click();
    await closePromise;
  });
});
