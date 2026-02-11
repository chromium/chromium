// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, LineFocusMovement, LineFocusStyle, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertHeadersForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
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

  test('has headers with flag', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    assertHeadersForDropdown(lineFocusMenu.$.menu, /*shouldHaveHeaders=*/ true);
  });

  test('no headers without flag', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    assertHeadersForDropdown(
        lineFocusMenu.$.menu, /*shouldHaveHeaders=*/ false);
  });

  test('line focus style prop update changes selected items', async () => {
    const window = LineFocusStyle.MEDIUM_WINDOW;
    lineFocusMenu.lineFocusStyle = window;
    await microtasksFinished();
    let selectedItems =
        lineFocusMenu.$.menu.menuItems.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(window, selectedItems[0]!.data, 'data');

    const off = LineFocusStyle.OFF;
    lineFocusMenu.lineFocusStyle = off;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuItems.filter(item => item.selected);
    assertEquals(1, selectedItems.length, 'selected off');
    assertEquals(off, selectedItems[0]!.data, 'data off');

    const line = LineFocusStyle.UNDERLINE;
    lineFocusMenu.lineFocusStyle = line;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuItems.filter(item => item.selected);
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
        lineFocusMenu.$.menu.menuItems.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(cursor, selectedItems[0]!.data);

    const staticMovement = LineFocusMovement.STATIC;
    lineFocusMenu.lineFocusMovement = staticMovement;
    await microtasksFinished();
    selectedItems =
        lineFocusMenu.$.menu.menuItems.filter(item => item.selected);
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

  test('restores saved line focus option', async () => {
    const lineFocus = chrome.readingMode.lineFocusSmallCursorWindow;
    const startingIndex = lineFocusMenu.$.menu.currentSelectedIndex;
    let styleSent = 0;
    let movementSent = 0;
    document.addEventListener(ToolbarEvent.LINE_FOCUS_STYLE, event => {
      styleSent = (event as CustomEvent).detail.data;
    });
    document.addEventListener(ToolbarEvent.LINE_FOCUS_MOVEMENT, event => {
      movementSent = (event as CustomEvent).detail.data;
    });

    lineFocusMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      lineFocus,
    };
    await microtasksFinished();

    assertEquals(startingIndex, lineFocusMenu.$.menu.currentSelectedIndex);
    const items =
        lineFocusMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.check-mark-showing-true');
    assertEquals(2, items.length);
    items[0]!.click();
    await microtasksFinished();
    assertEquals(LineFocusStyle.SMALL_WINDOW, styleSent);
    items[1]!.click();
    await microtasksFinished();
    assertEquals(LineFocusMovement.CURSOR, movementSent);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = lineFocusMenu.$.menu.currentSelectedIndex;

    lineFocusMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      lineFocus: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, lineFocusMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    lineFocusMenu.open(document.body);
    assertTrue(lineFocusMenu.$.menu.$.lazyMenu.get().open);
    lineFocusMenu.close();
    assertFalse(lineFocusMenu.$.menu.$.lazyMenu.get().open);
  });
});
