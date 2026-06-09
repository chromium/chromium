// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {GroupedActionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertTestSettingsAreNotDefaultSettings, getItemsInMenu, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('GroupedActionMenuElement', () => {
  let menu: GroupedActionMenuElement;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    mockMetrics();

    menu = document.createElement('grouped-action-menu');
    document.body.appendChild(menu);
    return microtasksFinished();
  });

  test('onClick sends group event name', async () => {
    let sentGroupEvent = false;
    menu.menuGroups = [
      {
        header: {title: 'Header 1', separator: false},
        items: [{title: 'Item 1', data: 1}],
        eventName: ToolbarEvent.THEME,
      },
    ];
    await microtasksFinished();
    document.addEventListener(ToolbarEvent.THEME, () => {
      sentGroupEvent = true;
    });

    const items = getItemsInMenu(menu.$.lazyMenu);
    assertTrue(items.length > 0);
    const item = items[0];
    assertTrue(!!item);
    item.click();
    await microtasksFinished();

    assertTrue(sentGroupEvent);
  });

  test('shows checkmark on selected items', async () => {
    menu.menuGroups = [
      {
        header: {title: 'Header 1', separator: false},
        items: [
          {title: 'Item 1', data: 1, selected: true},
          {title: 'Item 2', data: 2},
        ],
        eventName: ToolbarEvent.THEME,
      },
      {
        header: {title: 'Header 2', separator: true},
        items: [
          {title: 'Item 3', data: 3, selected: true},
          {title: 'Item 4', data: 4},
        ],
        eventName: ToolbarEvent.LINE_SPACING,
      },
    ];
    await microtasksFinished();

    const items = menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
        '.check-mark-showing-true');
    assertEquals(2, items.length);
    assertEquals('Item 1', items[0]!.parentElement!.textContent.trim());
    assertEquals('Item 3', items[1]!.parentElement!.textContent.trim());
  });

  test('renders headers correctly', async () => {
    menu.menuGroups = [
      {
        header: {title: 'Header 1', separator: false},
        items: [{title: 'Item 1', data: 1}],
        eventName: ToolbarEvent.THEME,
      },
    ];
    await microtasksFinished();

    const header =
        menu.$.lazyMenu.get().querySelector<HTMLElement>('.header-style');
    assertTrue(!!header);
    assertEquals('Header 1', header.textContent.trim());
  });
});
