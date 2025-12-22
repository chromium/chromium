// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineFocusMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertHeadersForDropdown, mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineFocusMenuElement', () => {
  let lineFocusMenu: LineFocusMenuElement;
  let metrics: TestMetricsBrowserProxy;

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

  test('line focus change', async () => {
    const window = chrome.readingMode.lineFocusThreeLineWindow;
    lineFocusMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_FOCUS, {detail: {data: window}}));
    assertEquals(window, chrome.readingMode.lineFocus);

    const off = chrome.readingMode.lineFocusOff;
    lineFocusMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_FOCUS, {detail: {data: off}}));
    assertEquals(off, chrome.readingMode.lineFocus);

    const line = chrome.readingMode.lineFocusCursorLine;
    lineFocusMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_FOCUS, {detail: {data: line}}));
    assertEquals(line, chrome.readingMode.lineFocus);

    assertEquals(
        ReadAnythingSettingsChange.LINE_FOCUS_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved line focus option', async () => {
    const lineFocus = chrome.readingMode.lineFocusOneLineWindow;
    const startingIndex = lineFocusMenu.$.menu.currentSelectedIndex;
    assertNotEquals(lineFocus, startingIndex);

    lineFocusMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: 0,
      speechRate: 0,
      font: '',
      highlightGranularity: 0,
      lineFocus,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, lineFocusMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = lineFocusMenu.$.menu.currentSelectedIndex;

    lineFocusMenu.settingsPrefs = {
      letterSpacing: 101,
      lineSpacing: 104,
      theme: 102,
      speechRate: 103,
      font: 'font',
      highlightGranularity: 103,
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
