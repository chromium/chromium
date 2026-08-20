// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppearanceMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent, VisualBrowserProxyImpl} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('AppearanceMenuElement', () => {
  let appearanceMenu: AppearanceMenuElement;
  let metrics: TestMetricsBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = mockMetrics();

    visualBrowserProxy = new TestVisualBrowserProxy();
    VisualBrowserProxyImpl.setInstance(visualBrowserProxy);

    appearanceMenu = document.createElement('appearance-menu');
    document.body.appendChild(appearanceMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(appearanceMenu);
  });

  test('theme prop update changes selected items', async () => {
    const yellowTheme = visualBrowserProxy.getYellowTheme();
    appearanceMenu.settingsPrefs = {
      ...appearanceMenu.settingsPrefs,
      theme: yellowTheme,
    };
    await microtasksFinished();

    const selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(yellowTheme, selectedItems[0]!.data, 'data');
  });

  test('on theme change', async () => {
    const numberOfThemes = 8;
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    const themesToTest = [
      visualBrowserProxy.getDefaultTheme(),
      visualBrowserProxy.getLightTheme(),
      visualBrowserProxy.getDarkTheme(),
      visualBrowserProxy.getYellowTheme(),
      visualBrowserProxy.getBlueTheme(),
      visualBrowserProxy.getHighContrastTheme(),
      visualBrowserProxy.getLowContrastLightTheme(),
      visualBrowserProxy.getLowContrastDarkTheme(),
    ];

    for (const testTheme of themesToTest) {
      visualBrowserProxy.resetResolver('onThemeChange');
      appearanceMenu.$.menu.dispatchEvent(
          new CustomEvent(ToolbarEvent.THEME, {detail: {data: testTheme}}));
      await microtasksFinished();

      assertEquals(
          testTheme, await visualBrowserProxy.whenCalled('onThemeChange'));
      const selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
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
    const color = visualBrowserProxy.getYellowTheme();
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertNotEquals(color, startingSelected?.data);

    appearanceMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      theme: color,
    };
    await microtasksFinished();

    const newSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(color, newSelected?.data);
    assertNotEquals(startingSelected?.data, newSelected?.data);
  });

  test('does nothing if saved color is the same', async () => {
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);

    appearanceMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      theme: 0,
    };
    await microtasksFinished();

    const newSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(startingSelected?.data, newSelected?.data);
  });

  test('presentation prop update changes selected items', async () => {
    appearanceMenu.presentationState =
        visualBrowserProxy.getInImmersiveOverlayPresentationState();
    await microtasksFinished();

    const selectedItems = appearanceMenu.$.menu.menuGroups[0]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(
        visualBrowserProxy.getInImmersiveOverlayPresentationState(),
        selectedItems[0]!.data);
  });

  test('on presentation change', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    const sidePanelState = visualBrowserProxy.getInSidePanelPresentationState();
    const immersiveState =
        visualBrowserProxy.getInImmersiveOverlayPresentationState();

    appearanceMenu.presentationState = sidePanelState;
    await microtasksFinished();

    appearanceMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: immersiveState}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
    assertEquals(1, visualBrowserProxy.getCallCount('togglePresentation'));

    appearanceMenu.presentationState = immersiveState;
    await microtasksFinished();

    appearanceMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.PRESENTATION_CHANGE, {detail: {data: sidePanelState}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
    assertEquals(2, visualBrowserProxy.getCallCount('togglePresentation'));
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    appearanceMenu.open(document.body);
    assertTrue(appearanceMenu.$.menu.$.lazyMenu.get().open);
    appearanceMenu.close();
    assertFalse(appearanceMenu.$.menu.$.lazyMenu.get().open);
  });
});
