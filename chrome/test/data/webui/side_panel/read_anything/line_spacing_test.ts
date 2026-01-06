// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertHeadersForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LineSpacing', () => {
  let lineSpacingMenu: LineSpacingMenuElement;
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

    lineSpacingMenu = document.createElement('line-spacing-menu');
    document.body.appendChild(lineSpacingMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineSpacingMenu);
  });

  test('does not have headers', () => {
    assertHeadersForDropdown(
        lineSpacingMenu.$.menu, /*shouldHaveHeaders=*/ false);
  });

  test('spacing change', async () => {
    const veryLoose = chrome.readingMode.veryLooseLineSpacing;
    const numberOfItems = 3;
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    lineSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_SPACING, {detail: {data: veryLoose}}));
    assertEquals(veryLoose, chrome.readingMode.lineSpacing);

    const loose = chrome.readingMode.looseLineSpacing;
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: loose}}));
    assertEquals(loose, chrome.readingMode.lineSpacing);

    const standard = chrome.readingMode.standardLineSpacing;
    lineSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LINE_SPACING, {detail: {data: standard}}));
    assertEquals(standard, chrome.readingMode.lineSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfItems, metrics.getCallCount('recordTextSettingsChange'));
    assertEquals(numberOfItems, closeAllMenusCount);
  });

  test('restores saved spacing option', async () => {
    const spacing = chrome.readingMode.veryLooseLineSpacing;
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    lineSpacingMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      lineSpacing: spacing,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, lineSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;

    lineSpacingMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      lineSpacing: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, lineSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    lineSpacingMenu.open(document.body);
    assertTrue(lineSpacingMenu.$.menu.$.lazyMenu.get().open);
    lineSpacingMenu.close();
    assertFalse(lineSpacingMenu.$.menu.$.lazyMenu.get().open);
  });
});
