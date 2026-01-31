// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {type SimpleActionMenuElement, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertTestSettingsAreNotDefaultSettings, getItemsInMenu, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('SimpleActionMenuElement', () => {
  let menu: SimpleActionMenuElement;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    mockMetrics();

    menu = document.createElement('simple-action-menu');
    document.body.appendChild(menu);
    return microtasksFinished();
  });

  test('onClick sends menu event name if no item event name', async () => {
    let sent = false;
    menu.eventName = ToolbarEvent.FONT_SIZE;
    menu.menuItems = [
      {title: 'Item 1', data: 1},
      {title: 'Item 2', data: 2},
    ];
    await microtasksFinished();
    document.addEventListener(ToolbarEvent.FONT_SIZE, () => {
      sent = true;
    });

    const items = getItemsInMenu(menu.$.lazyMenu);
    assertTrue(items.length > 0);
    const item = items[0];
    assertTrue(!!item);
    item.click();
    await microtasksFinished();

    assertTrue(sent);
  });

  test('onClick sends item event name if present', async () => {
    let sentMenuEvent = false;
    let sentItemEvent = false;
    menu.eventName = ToolbarEvent.FONT_SIZE;
    menu.menuItems = [
      {title: 'Item 1', data: 1, eventName: ToolbarEvent.THEME},
      {title: 'Item 2', data: 2},
    ];
    await microtasksFinished();
    document.addEventListener(ToolbarEvent.THEME, () => {
      sentItemEvent = true;
    });
    document.addEventListener(ToolbarEvent.FONT_SIZE, () => {
      sentMenuEvent = true;
    });

    const items = getItemsInMenu(menu.$.lazyMenu);
    assertTrue(items.length > 0);
    const item = items[0];
    assertTrue(!!item);
    item.click();
    await microtasksFinished();

    assertTrue(sentItemEvent);
    assertFalse(sentMenuEvent);
  });

  test(
      'shows checkmark on selected index if not marked a selected item',
      async () => {
        menu.eventName = ToolbarEvent.FONT_SIZE;
        menu.menuItems = [
          {title: 'Item 1', data: 1},
          {title: 'Item 2', data: 2},
        ];
        menu.currentSelectedIndex = 1;
        await microtasksFinished();

        const items = menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.check-mark-showing-true');
        assertEquals(1, items.length);
        assertEquals('Item 2', items[0]!.parentElement!.textContent.trim());
      });

  test('shows checkmark on selected items', async () => {
    menu.eventName = ToolbarEvent.FONT_SIZE;
    menu.menuItems = [
      {title: 'Item 1', data: 1, selected: true},
      {title: 'Item 2', data: 2},
      {title: 'Item 3', data: 2, selected: true},
      {title: 'Item 4', data: 2},
    ];
    menu.currentSelectedIndex = -1;
    await microtasksFinished();

    const items = menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
        '.check-mark-showing-true');
    assertEquals(2, items.length);
    assertEquals('Item 1', items[0]!.parentElement!.textContent.trim());
    assertEquals('Item 3', items[1]!.parentElement!.textContent.trim());
  });
});
