// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {LineFocusMovement, LineFocusStyle, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineFocusMenuElement', () => {
  let lineFocusMenu: LineFocusMenuElement;
  let metrics: TestMetricsBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    lineFocusMenu = document.createElement('line-focus-menu');
    document.body.appendChild(lineFocusMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineFocusMenu);
  });



  test('line focus style prop update changes selected items', async () => {
    const window = LineFocusStyle.MEDIUM_WINDOW;
    lineFocusMenu.lineFocusStyle = window;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(window, selectedItems[0]!.data, 'data');

    const off = LineFocusStyle.OFF;
    lineFocusMenu.lineFocusStyle = off;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected off');
    assertEquals(off, selectedItems[0]!.data, 'data off');

    const line = LineFocusStyle.UNDERLINE;
    lineFocusMenu.lineFocusStyle = line;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected line');
    assertEquals(line, selectedItems[0]!.data, 'data line');
  });

  test('on line focus style change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.LARGE_WINDOW}}));
    await microtasksFinished();

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('line focus movement prop update changes selected items', async () => {
    const cursor = LineFocusMovement.CURSOR;
    lineFocusMenu.lineFocusMovement = cursor;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(cursor, selectedItems[0]!.data);

    const staticMovement = LineFocusMovement.STATIC;
    lineFocusMenu.lineFocusMovement = staticMovement;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(staticMovement, selectedItems[0]!.data);
  });

  test('on line focus movement change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineFocusMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.CURSOR}}));
    await microtasksFinished();

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    lineFocusMenu.open(document.body);
    assertTrue(lineFocusMenu.$.menu.$.lazyMenu.get().open);
    lineFocusMenu.close();
    assertFalse(lineFocusMenu.$.menu.$.lazyMenu.get().open);
  });
});
