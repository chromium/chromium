// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ColorMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('ColorMenuElement', () => {
  let colorMenu: ColorMenuElement;
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

    colorMenu = document.createElement('color-menu');
    document.body.appendChild(colorMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(colorMenu);
  });



  test('theme change', async () => {
    const numberOfThemes = 6;

    const theme1 = chrome.readingMode.blueTheme;
    const closePromise1 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme1}}));
    await closePromise1;
    assertEquals(theme1, chrome.readingMode.colorTheme);

    const theme2 = chrome.readingMode.defaultTheme;
    const closePromise2 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme2}}));
    await closePromise2;
    assertEquals(theme2, chrome.readingMode.colorTheme);

    const theme3 = chrome.readingMode.darkTheme;
    const closePromise3 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme3}}));
    await closePromise3;
    assertEquals(theme3, chrome.readingMode.colorTheme);

    const theme4 = chrome.readingMode.highContrastTheme;
    const closePromise4 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme4}}));
    await closePromise4;
    assertEquals(theme4, chrome.readingMode.colorTheme);

    const theme6 = chrome.readingMode.lowContrastLightTheme;
    const closePromise5 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme6}}));
    await closePromise5;
    assertEquals(theme6, chrome.readingMode.colorTheme);

    const theme7 = chrome.readingMode.lowContrastDarkTheme;
    const closePromise6 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.THEME, {detail: {data: theme7}}));
    await closePromise6;
    assertEquals(theme7, chrome.readingMode.colorTheme);

    assertEquals(
        ReadAnythingSettingsChange.THEME_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfThemes, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved color option', async () => {
    const color = chrome.readingMode.yellowTheme;
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;
    assertNotEquals(color, startingIndex);

    colorMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      theme: color,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, colorMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved color is the same', async () => {
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;

    colorMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      theme: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, colorMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    colorMenu.open(document.body);
    assertTrue(colorMenu.$.menu.$.lazyMenu.get().open);
    colorMenu.close();
    assertFalse(colorMenu.$.menu.$.lazyMenu.get().open);
  });
});
