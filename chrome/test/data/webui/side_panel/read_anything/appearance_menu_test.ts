// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppearanceMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('AppearanceMenuElement', () => {
  let appearanceMenu: AppearanceMenuElement;
  let metrics: TestMetricsBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    appearanceMenu = document.createElement('appearance-menu');
    document.body.appendChild(appearanceMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(appearanceMenu);
  });

  test('theme prop update changes selected items', async () => {
    const yellowTheme = chrome.readingMode.yellowTheme;
    appearanceMenu.settingsPrefs = {
      ...appearanceMenu.settingsPrefs,
      theme: yellowTheme,
    };
    await microtasksFinished();

    const selectedItems = appearanceMenu.$.menu.menuGroups[0]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(yellowTheme, selectedItems[0]!.data, 'data');
  });

  test('on theme change', async () => {
    const numberOfThemes = 8;
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    let calledTheme = -1;
    chrome.readingMode.onThemeChange = (val: number) => calledTheme = val;

    const themesToTest = [
      chrome.readingMode.defaultTheme,
      chrome.readingMode.lightTheme,
      chrome.readingMode.darkTheme,
      chrome.readingMode.yellowTheme,
      chrome.readingMode.blueTheme,
      chrome.readingMode.highContrastTheme,
      chrome.readingMode.lowContrastLightTheme,
      chrome.readingMode.lowContrastDarkTheme,
    ];

    for (const testTheme of themesToTest) {
      appearanceMenu.$.menu.dispatchEvent(
          new CustomEvent(ToolbarEvent.THEME, {detail: {data: testTheme}}));
      await microtasksFinished();

      assertEquals(testTheme, calledTheme);
      const selectedItems = appearanceMenu.$.menu.menuGroups[0]!.items.filter(
          item => item.selected);
      assertEquals(1, selectedItems.length);
      assertEquals(testTheme, selectedItems[0]!.data);
    }

    assertEquals(
        ReadAnythingSettingsChange.THEME_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfThemes, metrics.getCallCount('recordTextSettingsChange'));
    assertEquals(0, closeAllMenusCount);
  });

  test('restores saved color option', async () => {
    const color = chrome.readingMode.yellowTheme;
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertNotEquals(color, startingSelected?.data);

    appearanceMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      theme: color,
    };
    await microtasksFinished();

    const newSelected =
        appearanceMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertEquals(color, newSelected?.data);
    assertNotEquals(startingSelected?.data, newSelected?.data);
  });

  test('does nothing if saved color is the same', async () => {
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);

    appearanceMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      theme: 0,
    };
    await microtasksFinished();

    const newSelected =
        appearanceMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertEquals(startingSelected?.data, newSelected?.data);
  });

  test('presentation prop update changes selected items', async () => {
    appearanceMenu.presentationState =
        chrome.readingMode.inImmersiveOverlayPresentationState;
    await microtasksFinished();

    const selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(
        chrome.readingMode.inImmersiveOverlayPresentationState,
        selectedItems[0]!.data);
  });

  test('on presentation change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    const sidePanelState = chrome.readingMode.inSidePanelPresentationState;
    const immersiveState =
        chrome.readingMode.inImmersiveOverlayPresentationState;
    chrome.readingMode.togglePresentation = () => {
      if (appearanceMenu.presentationState === sidePanelState) {
        appearanceMenu.presentationState = immersiveState;
      } else if (appearanceMenu.presentationState === immersiveState) {
        appearanceMenu.presentationState = sidePanelState;
      }
    };

    appearanceMenu.presentationState = sidePanelState;
    await microtasksFinished();

    appearanceMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: immersiveState}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
    assertEquals(immersiveState, appearanceMenu.presentationState);
    let selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(immersiveState, selectedItems[0]!.data);

    appearanceMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: sidePanelState}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
    assertEquals(sidePanelState, appearanceMenu.presentationState);
    selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(sidePanelState, selectedItems[0]!.data);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    appearanceMenu.open(document.body);
    assertTrue(appearanceMenu.$.menu.$.lazyMenu.get().open);
    appearanceMenu.close();
    assertFalse(appearanceMenu.$.menu.$.lazyMenu.get().open);
  });
});
