// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionHeaderElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerListSectionHeaderTest', () => {
  let header: OrganizerListSectionHeaderElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    header = document.createElement('organizer-list-section-header');
    header.textContent = 'Open Tabs';
    header.expanded = true;
    document.body.appendChild(header);
    await microtasksFinished();
  });

  test('has options tooltip and aria-label on menu button', () => {
    const menuButton = header.$.menuButton;
    assertEquals('Options', menuButton.title);
    assertEquals('Options', menuButton.getAttribute('aria-label'));
  });

  test('has expand/collapse tooltip on expand button', async () => {
    const expandButton = header.$.expandButton;
    assertEquals('Collapse', expandButton.title);

    header.expanded = false;
    await microtasksFinished();
    assertEquals('Expand', expandButton.title);

    header.expanded = true;
    await microtasksFinished();
    assertEquals('Collapse', expandButton.title);
  });

  test('toggles expanded state when expand button is clicked', async () => {
    const expandButton = header.$.expandButton;
    const expandIcon = expandButton.$.icon;

    assertTrue(header.expanded);
    assertTrue(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-up', expandIcon.ironIcon);
    assertEquals('true', expandIcon.getAttribute('aria-expanded'));

    let whenExpandedChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', header);
    expandButton.click();
    let event = await whenExpandedChanged;
    await microtasksFinished();

    assertFalse(event.detail.value);
    assertFalse(header.expanded);
    assertFalse(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-down', expandIcon.ironIcon);
    assertEquals('false', expandIcon.getAttribute('aria-expanded'));

    whenExpandedChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', header);
    expandButton.click();
    event = await whenExpandedChanged;
    await microtasksFinished();

    assertTrue(event.detail.value);
    assertTrue(header.expanded);
    assertTrue(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-up', expandIcon.ironIcon);
    assertEquals('true', expandIcon.getAttribute('aria-expanded'));
  });

  test(
      'hides expand icon and disables expand button when disabled',
      async () => {
        const expandButton = header.$.expandButton;
        const expandIcon = expandButton.$.icon;

        assertFalse(expandButton.disabled);
        assertEquals('flex', getComputedStyle(expandIcon).display);

        header.disabled = true;
        await microtasksFinished();

        assertTrue(expandButton.disabled);
        assertEquals('none', getComputedStyle(expandIcon).display);
      });

  test('shows menu button only when expanded and enabled', async () => {
    const menuButton = header.$.menuButton;

    assertTrue(isVisible(menuButton));

    header.expanded = false;
    await microtasksFinished();
    assertFalse(isVisible(menuButton));

    header.expanded = true;
    header.disabled = true;
    await microtasksFinished();
    assertFalse(isVisible(menuButton));

    header.disabled = false;
    await microtasksFinished();
    assertTrue(isVisible(menuButton));
  });

  test('opens menu with show most recent and show all options', async () => {
    const menu = header.$.menu;
    const menuButton = header.$.menuButton;
    const showSomeButton = header.$.showSomeButton;
    const showAllButton = header.$.showAllButton;

    assertFalse(menu.open);
    assertEquals('Show most recent', showSomeButton.textContent.trim());
    assertEquals('Show all', showAllButton.textContent.trim());
    assertEquals('menuitemradio', showSomeButton.getAttribute('role'));
    assertEquals('menuitemradio', showAllButton.getAttribute('role'));
    assertEquals('true', showSomeButton.getAttribute('aria-checked'));
    assertEquals('false', showAllButton.getAttribute('aria-checked'));
    assertFalse(header.showAll);
    assertFalse(header.$.showSomeCheck.hasAttribute('invisible'));
    assertTrue(header.$.showAllCheck.hasAttribute('invisible'));

    menuButton.click();
    await microtasksFinished();

    assertTrue(menu.open);
    assertTrue(header.expanded);
    assertEquals(
        menuButton.getBoundingClientRect().bottom,
        menu.getDialog().getBoundingClientRect().top);

    let whenShowAllChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'show-all-changed', header);
    showAllButton.click();
    let event = await whenShowAllChanged;
    await microtasksFinished();
    assertTrue(event.detail.value);
    assertTrue(header.showAll);
    assertEquals('false', showSomeButton.getAttribute('aria-checked'));
    assertEquals('true', showAllButton.getAttribute('aria-checked'));
    assertTrue(header.$.showSomeCheck.hasAttribute('invisible'));
    assertFalse(header.$.showAllCheck.hasAttribute('invisible'));
    assertFalse(menu.open);

    menuButton.click();
    await microtasksFinished();
    assertTrue(menu.open);

    whenShowAllChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'show-all-changed', header);
    showSomeButton.click();
    event = await whenShowAllChanged;
    await microtasksFinished();
    assertFalse(event.detail.value);
    assertFalse(header.showAll);
    assertEquals('true', showSomeButton.getAttribute('aria-checked'));
    assertEquals('false', showAllButton.getAttribute('aria-checked'));
    assertFalse(header.$.showSomeCheck.hasAttribute('invisible'));
    assertTrue(header.$.showAllCheck.hasAttribute('invisible'));
    assertFalse(menu.open);
  });

  test('updates check mark visibility when showAll changes', async () => {
    header.showAll = false;
    await microtasksFinished();
    assertFalse(header.$.showSomeCheck.hasAttribute('invisible'));
    assertTrue(header.$.showAllCheck.hasAttribute('invisible'));

    header.showAll = true;
    await microtasksFinished();
    assertTrue(header.$.showSomeCheck.hasAttribute('invisible'));
    assertFalse(header.$.showAllCheck.hasAttribute('invisible'));
  });

  test('collapses and expands with left and right arrow keys', async () => {
    header.expanded = true;
    await microtasksFinished();

    header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowLeft',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertFalse(header.expanded);

    header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowRight',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertTrue(header.expanded);
  });

  test(
      'does not collapse or expand with arrow keys when disabled', async () => {
        header.disabled = true;
        header.expanded = true;
        await microtasksFinished();

        header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowLeft',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertTrue(header.expanded);

        header.expanded = false;
        await microtasksFinished();

        header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowRight',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(header.expanded);
      });

  test('tab navigation on expand button', async () => {
    header.expanded = true;
    await microtasksFinished();

    // Tab on expand button focuses menu button when visible
    header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    assertEquals(header.$.menuButton, getDeepActiveElement());

    // When menu button is hidden (section collapsed), Tab fires
    // focus-next-section
    header.expanded = false;
    await microtasksFinished();

    const nextSectionPromise = eventToPromise('focus-next-section', header);
    header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await nextSectionPromise;

    // Shift+Tab fires focus-previous-section
    const prevSectionPromise = eventToPromise('focus-previous-section', header);
    header.$.expandButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await prevSectionPromise;
  });

  test('tab navigation on menu button', async () => {
    header.expanded = true;
    await microtasksFinished();

    // Shift+Tab on menu button returns focus to expand button
    header.focusMenuButton();
    assertEquals(header.$.menuButton, getDeepActiveElement());

    header.$.menuButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    assertEquals(header.$.expandButton.$.icon, getDeepActiveElement());

    // Tab on menu button fires focus-next-section
    const nextSectionPromise = eventToPromise('focus-next-section', header);
    header.$.menuButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await nextSectionPromise;
  });

  test('focus method delegates to expand button', () => {
    header.focus();
    assertEquals(header.$.expandButton.$.icon, getDeepActiveElement());
  });

  test('disables focus ring on expand button icon', async () => {
    document.documentElement.classList.add('focus-outline-visible');
    header.focus();
    await microtasksFinished();

    const expandIcon = header.$.expandButton.$.icon;
    assertEquals(
        'transparent',
        getComputedStyle(expandIcon)
            .getPropertyValue('--cr-icon-button-focus-outline-color')
            .trim());
    assertEquals('none', getComputedStyle(expandIcon).boxShadow);
    document.documentElement.classList.remove('focus-outline-visible');
  });
});
