// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {KEYBOARD_NAV_CLASS, MENU_SHOW_DELAY_MS, SUBMENU_SHOW_DELAY_MS} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {SettingsMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {SettingsOption, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome-untrusted://webui-test/keyboard_mock_interactions.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';


suite('SettingsMenuElement', () => {
  let settingsMenu: SettingsMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.imagesFeatureEnabled = true;
    chrome.readingMode.isLineFocusEnabled = true;

    settingsMenu = document.createElement('settings-menu');
    settingsMenu.id = 'settingsMenu';
    document.body.appendChild(settingsMenu);

    const anchor = document.createElement('div');
    document.body.appendChild(anchor);
    settingsMenu.open(anchor);

    settingsMenu.$.lazyMenu.get();
    await microtasksFinished();
  });

  test('click outside fires close-all-menus event', () => {
    let closeWasCalled = false;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeWasCalled = true);

    document.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      composed: true,
      cancelable: true,
      view: window,
    }));
    assertTrue(
        closeWasCalled,
        'Clicking outside should fire the close-all-menus event');
  });

  test('click inside does NOT fires close-all-menus event', () => {
    let closeWasCalled = false;
    settingsMenu.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeWasCalled = true);

    const internalMenu = settingsMenu.$.lazyMenu.get();
    internalMenu.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      composed: true,
      cancelable: true,
      view: window,
    }));
    assertFalse(
        closeWasCalled,
        'Clicking inside should NOT fire the close-all-menus event');
  });

  test('open settings submenu event is fired when menu item is clicked', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        actionMenu.querySelectorAll<HTMLButtonElement>('.menu-row');

    let submenuEvents = 0;
    settingsMenu.addEventListener(
        ToolbarEvent.OPEN_SETTINGS_SUBMENU, () => submenuEvents++);

    for (const item of menuItems) {
      item.click();
    }

    assertEquals(8, submenuEvents);
  });

  test(
      'open settings submenu event is fired when menu items are hovered',
      () => {
        const actionMenu = settingsMenu.$.lazyMenu.get();
        const menuItems =
            actionMenu.querySelectorAll<HTMLButtonElement>('.menu-row');

        let submenuEvents = 0;
        settingsMenu.addEventListener(
            ToolbarEvent.OPEN_SETTINGS_SUBMENU, () => submenuEvents++);

        const timer = new MockTimer();
        timer.install();
        for (const item of menuItems) {
          item.dispatchEvent(new PointerEvent(
              'pointerenter', {bubbles: true, cancelable: true, view: window}));
          timer.tick(SUBMENU_SHOW_DELAY_MS + 10);
        }
        timer.uninstall();

        assertEquals(8, submenuEvents);
      });

  test('links event is fired when links item is clicked', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        Array.from(actionMenu.querySelectorAll<HTMLButtonElement>('.menu-row'));
    const targetItem = menuItems.find(item => item.id === SettingsOption.LINKS);
    assertTrue(!!targetItem);

    let linksEventWasFired = false;
    settingsMenu.addEventListener(
        ToolbarEvent.LINKS, () => linksEventWasFired = true);
    let linkEnabledTogled = false;
    chrome.readingMode.onLinksEnabledToggled = () => linkEnabledTogled = true;

    targetItem.click();
    assertTrue(linksEventWasFired);
    assertTrue(linkEnabledTogled);
  });

  test('images event is fired when images item is clicked', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        Array.from(actionMenu.querySelectorAll<HTMLButtonElement>('.menu-row'));
    const targetItem =
        menuItems.find(item => item.id === SettingsOption.IMAGES);
    assertTrue(!!targetItem);

    let imagesEventWasFired = false;
    settingsMenu.addEventListener(
        ToolbarEvent.IMAGES, () => imagesEventWasFired = true);
    let imagesEnabledTogled = false;
    chrome.readingMode.onImagesEnabledToggled = () => imagesEnabledTogled =
        true;

    targetItem.click();
    assertTrue(imagesEventWasFired);
    assertTrue(imagesEnabledTogled);
  });

  test('moving the mouse removes keyboard-nav class', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    actionMenu.classList.add(KEYBOARD_NAV_CLASS);
    assertTrue(actionMenu.classList.contains(KEYBOARD_NAV_CLASS));

    actionMenu.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    actionMenu.classList.remove(KEYBOARD_NAV_CLASS);
  });

  test('mouse leave clears open timer', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        actionMenu.querySelectorAll<HTMLButtonElement>('.menu-row');
    const targetItem = menuItems[0];
    assertTrue(!!targetItem);

    let submenuEvents = 0;
    settingsMenu.addEventListener(
        ToolbarEvent.OPEN_SETTINGS_SUBMENU, ((event: CustomEvent) => {
                                              if (event.detail.id) {
                                                submenuEvents++;
                                              }
                                            }) as EventListener);

    const timer = new MockTimer();
    timer.install();

    // Hover over the item
    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));

    // Leave the item before the timer fires
    targetItem.dispatchEvent(new PointerEvent(
        'pointerleave', {bubbles: true, cancelable: true, view: window}));

    // Advance the timer past the delay
    timer.tick(MENU_SHOW_DELAY_MS + 10);
    timer.uninstall();

    assertEquals(0, submenuEvents, 'Submenu event should not have fired');
  });

  test(
      'pressing escape or back horizontal arrow fires close submenu event',
      async () => {
        const actionMenu = settingsMenu.$.lazyMenu.get();
        const menuItems =
            Array.from(actionMenu.querySelectorAll<HTMLElement>('.menu-row'));
        const targetItem =
            menuItems.find(item => item.id === SettingsOption.FONT);
        assertTrue(!!targetItem);
        targetItem.click();

        const whenFired =
            eventToPromise(ToolbarEvent.CLOSE_SUBMENU_REQUESTED, settingsMenu);
        keyDownOn(settingsMenu, 0, undefined, 'Escape');
        await whenFired;

        targetItem.click();
        keyDownOn(settingsMenu, 0, undefined, 'ArrowLeft');
        await whenFired;
      });

  test('closing the menu prevents open timer to be fired', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems =
        Array.from(actionMenu.querySelectorAll<HTMLElement>('.menu-row'));
    const targetItem =
        menuItems.find(item => item.id === SettingsOption.LINE_SPACING);
    assertTrue(!!targetItem);

    let openSubmenuWasFiredAfterClose = false;
    actionMenu.addEventListener(ToolbarEvent.OPEN_SETTINGS_SUBMENU, () => {
      openSubmenuWasFiredAfterClose = true;
    });

    const timer = new MockTimer();
    targetItem.dispatchEvent(new PointerEvent(
        'pointerenter', {bubbles: true, cancelable: true, view: window}));
    timer.tick(SUBMENU_SHOW_DELAY_MS - 1);
    actionMenu.close();
    timer.tick(1);

    assertFalse(openSubmenuWasFiredAfterClose);
    timer.uninstall();
  });

  test('opening the menu fires a settings-opened event', async () => {
    settingsMenu.close();

    const whenFired =
        eventToPromise(ToolbarEvent.SETTINGS_OPENED, settingsMenu);
    const anchor = document.createElement('div');
    document.body.appendChild(anchor);
    settingsMenu.open(anchor);
    await whenFired;
  });

  test('closing the menu fires a settings-closed event', async () => {
    const whenFired =
        eventToPromise(ToolbarEvent.SETTINGS_CLOSED, settingsMenu);
    settingsMenu.close();
    await whenFired;
  });
});
