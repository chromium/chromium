// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {GroupedActionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertTestSettingsAreNotDefaultSettings, getItemsInMenu, setupTestEnvironment} from './common.js';

suite('GroupedActionMenuElement', () => {
  let menu: GroupedActionMenuElement;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    setupTestEnvironment();

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

  test('onClick sends item event name when provided', async () => {
    const testItemEvent = 'test-item-event';
    menu.menuGroups = [
      {
        header: {title: 'Header 1', separator: false},
        items: [
          {title: 'Item 1', data: 1, eventName: testItemEvent},
          {title: 'Item 2', data: 2},
        ],
        eventName: ToolbarEvent.THEME,
      },
      {
        header: {title: 'Header 2', separator: true},
        items: [
          {title: 'Item 3', data: 3, eventName: 'group-less-item-event'},
        ],
      },
    ];
    await microtasksFinished();

    const items = getItemsInMenu(menu.$.lazyMenu);

    assertEquals(3, items.length);

    // Clicking item with eventName sends item eventName.
    const whenItemFired =
        eventToPromise<CustomEvent<{data: number}>>(testItemEvent, menu);
    items[0]!.click();
    const itemEvent = await whenItemFired;

    assertEquals(1, itemEvent.detail.data);

    // Clicking item without eventName falls back to group eventName.
    const whenGroupFired =
        eventToPromise<CustomEvent<{data: number}>>(ToolbarEvent.THEME, menu);
    items[1]!.click();
    const groupEvent = await whenGroupFired;

    assertEquals(2, groupEvent.detail.data);

    // Clicking item in group without eventName sends item eventName.
    const whenGroupLessFired = eventToPromise<CustomEvent<{data: number}>>(
        'group-less-item-event', menu);
    items[2]!.click();
    const groupLessEvent = await whenGroupLessFired;

    assertEquals(3, groupLessEvent.detail.data);
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

  test('button accessible name and title fall back to item title', async () => {
    menu.menuGroups = [
      {
        header: {title: 'Header 1', separator: false},
        items: [
          {title: 'Default Title', data: 1},
          {title: 'Custom Title', ariaLabel: 'Custom Label', data: 2},
        ],
        eventName: ToolbarEvent.THEME,
      },
    ];
    await microtasksFinished();

    const buttons = getItemsInMenu(menu.$.lazyMenu);

    assertEquals(2, buttons.length);

    // Default title fallback.
    assertEquals('Default Title', buttons[0]!.getAttribute('aria-label'));
    assertEquals('Default Title', buttons[0]!.getAttribute('title'));

    // Explicit ariaLabel.
    assertEquals('Custom Label', buttons[1]!.getAttribute('aria-label'));
    assertEquals('Custom Label', buttons[1]!.getAttribute('title'));
  });
});
