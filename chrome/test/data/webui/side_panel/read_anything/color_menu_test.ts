// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ColorMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('ColorMenuElement', () => {
  let colorMenu: ColorMenuElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    colorMenu = document.createElement('color-menu');
    document.body.appendChild(colorMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(colorMenu);
  });

  test('theme change', async () => {
    const theme1 = chrome.readingMode.blueTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme1}}));
    assertEquals(theme1, chrome.readingMode.colorTheme);

    const theme2 = chrome.readingMode.defaultTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme2}}));
    assertEquals(theme2, chrome.readingMode.colorTheme);

    const theme3 = chrome.readingMode.darkTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme3}}));
    assertEquals(theme3, chrome.readingMode.colorTheme);

    const theme4 = chrome.readingMode.highContrastTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme4}}));
    assertEquals(theme4, chrome.readingMode.colorTheme);

    const theme5 = chrome.readingMode.lowContrastTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme5}}));
    assertEquals(theme5, chrome.readingMode.colorTheme);

    const theme6 = chrome.readingMode.sepiaLightTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme6}}));
    assertEquals(theme6, chrome.readingMode.colorTheme);

    const theme7 = chrome.readingMode.sepiaDarkTheme;
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme7}}));
    assertEquals(theme7, chrome.readingMode.colorTheme);

    assertEquals(
        ReadAnythingSettingsChange.THEME_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(7, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved color option', async () => {
    const color = chrome.readingMode.yellowTheme;
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;
    assertNotEquals(color, startingIndex);

    colorMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: color,
      speechRate: 0,
      font: '',
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, colorMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved color is the same', async () => {
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;

    colorMenu.settingsPrefs = {
      letterSpacing: 100,
      lineSpacing: 101,
      theme: 0,
      speechRate: 103,
      font: 'font',
      highlightGranularity: 103,
    };
    await microtasksFinished();

    assertEquals(startingIndex, colorMenu.$.menu.currentSelectedIndex);
  });
});
