// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {MENU_SHOW_DELAY_MS} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement, SettingsMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {SettingsOption} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome-untrusted://webui-test/keyboard_mock_interactions.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('Toolbar Settings Menu', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;
  let menuButton: CrIconButtonElement;
  let settingsMenu: SettingsMenuElement;

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    assertTrue(!!toolbar.shadowRoot);
    shadowRoot = toolbar.shadowRoot;
  }

  function getButton(id: string): CrIconButtonElement|null {
    return shadowRoot.querySelector<CrIconButtonElement>('#' + id);
  }

  function getMenuItem(id: SettingsOption): HTMLElement|null {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        Array.from(actionMenu.querySelectorAll<HTMLElement>('.menu-row'));
    const item = menuItems.find(item => item.id === id);
    if (item instanceof HTMLElement) {
      return item;
    }

    return null;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isImmersiveEnabled = true;
    stubAnimationFrame();
    await createToolbar();

    const moreButton = getButton('more');
    assertTrue(!!moreButton);
    menuButton = moreButton;

    settingsMenu = toolbar.$.settingsMenu;
    menuButton.click();

    await microtasksFinished();
  });

  teardown(async () => {
    if (settingsMenu) {
      const lazyMenu = settingsMenu.$.lazyMenu.getIfExists();
      if (lazyMenu && lazyMenu.open) {
        settingsMenu.close();
      }
    }
    await microtasksFinished();
  });

  test('settings is dropdown menu for more', () => {
    assertTrue(settingsMenu.$.lazyMenu.get().open);
  });

  test('settings menu opens submenus on click', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    targetItem.click();
    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });

  test('settings menu opens submenus on hover', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(MENU_SHOW_DELAY_MS + 10);
    timer.uninstall();
    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });

  test('settings menu does not open submenu before delay', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(MENU_SHOW_DELAY_MS - 10);

    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    timer.uninstall();
  });

  test('menus are closed when clicked out of them', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(MENU_SHOW_DELAY_MS + 10);
    timer.uninstall();

    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);

    menuButton.click();
    const actionMenu = settingsMenu.$.lazyMenu.get();
    assertFalse(actionMenu.open);
    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });

  test('opened menu is closed if mouse is moved out of the menus', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(MENU_SHOW_DELAY_MS + 10);

    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);

    targetItem.dispatchEvent(new PointerEvent('pointerleave', {
      bubbles: true,
      cancelable: true,
      view: window,
      relatedTarget: menuButton,
    }));
    timer.tick(MENU_SHOW_DELAY_MS + 10);
    timer.uninstall();

    const actionMenu = settingsMenu.$.lazyMenu.get();
    assertTrue(actionMenu.open);
    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });

  test('clicking on language menu does not close settings menu', async () => {
    // Open the voice selection menu
    const targetItem = getMenuItem(SettingsOption.VOICE_SELECTION);
    assertTrue(!!targetItem);
    targetItem.click();
    assertTrue(toolbar.$.voiceSelectionMenu.$.voiceSelectionMenu.get().open);

    // Open the language menu
    const languageMenuButton =
        toolbar.$.voiceSelectionMenu.shadowRoot.querySelector<HTMLElement>(
            '.language-menu-button');
    assertTrue(!!languageMenuButton);
    languageMenuButton.click();
    await microtasksFinished();

    const languageMenu =
        toolbar.$.voiceSelectionMenu.shadowRoot.querySelector('language-menu');
    assertTrue(!!languageMenu);
    const dialog = languageMenu.$.languageMenu;

    dialog.click();

    assertTrue(settingsMenu.$.lazyMenu.get().open);
  });

  test('sending a key event cancels open timer', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    const elapsedTime = MENU_SHOW_DELAY_MS - 10;
    timer.tick(elapsedTime);

    keyDownOn(settingsMenu, 0, undefined, 'ArrowDown');
    timer.tick(elapsedTime + 20);
    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    timer.uninstall();
  });

  test(
      'sending horizontal forward arrow event opens submenu of focused element',
      () => {
        const targetItem = getMenuItem(SettingsOption.FONT);
        assertTrue(!!targetItem);
        targetItem.focus();

        keyDownOn(settingsMenu, 0, undefined, 'ArrowRight');
        assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
      });

  test('sending vertical forward arrow focuses a different element', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    targetItem.focus();

    keyDownOn(settingsMenu, 0, undefined, 'ArrowUp');
    assertNotEquals(document.activeElement, targetItem);
  });

  test('sending a arrow event does nothing on toggle items', () => {
    const targetItem = getMenuItem(SettingsOption.LINKS);
    assertTrue(!!targetItem);
    targetItem.focus();

    let didToggleLinks = false;
    chrome.readingMode.onLinksEnabledToggled = () => {
      didToggleLinks = true;
    };
    keyDownOn(settingsMenu, 0, undefined, 'ArrowRight');
    assertFalse(didToggleLinks);
  });

  test(
      'sending horizontal backward arrow or escape key closes submenu if open',
      () => {
        const targetItem = getMenuItem(SettingsOption.FONT);
        const targetMenu = toolbar.$.fontMenu.$.menu.$.lazyMenu.get();
        assertTrue(!!targetItem);

        targetItem.click();
        assertTrue(targetMenu.open);
        keyDownOn(settingsMenu, 0, undefined, 'ArrowLeft');
        assertFalse(targetMenu.open);

        targetItem.click();
        assertTrue(targetMenu.open);
        keyDownOn(settingsMenu, 0, undefined, 'Escape');
        assertFalse(targetMenu.open);
      });

  test('submenus should not open after settings menu is closed', () => {
    const targetItem = getMenuItem(SettingsOption.FONT);
    assertTrue(!!targetItem);
    const timer = new MockTimer();
    timer.install();

    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(MENU_SHOW_DELAY_MS - 1);
    settingsMenu.close();
    timer.tick(1);

    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    timer.uninstall();
  });

  test('close button appears in immersive mode', async () => {
    toolbar.isImmersiveMode = true;
    await microtasksFinished();
    const closeButton = getButton('close');
    assertTrue(!!closeButton);
  });

  test('close button does not appear in side panel mode', () => {
    const closeButton = getButton('close');
    assertFalse(!!closeButton);
  });
});
