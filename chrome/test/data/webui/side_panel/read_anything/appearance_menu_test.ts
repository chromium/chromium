// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppearanceMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('AppearanceMenuElement', () => {
  let appearanceMenu: AppearanceMenuElement;
  let visualBrowserProxy: TestVisualBrowserProxy;

  const originalMatchMedia = window.matchMedia;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

    appearanceMenu = document.createElement('appearance-menu');
    document.body.appendChild(appearanceMenu);
  });

  teardown(() => {
    window.matchMedia = originalMatchMedia;
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(appearanceMenu);
  });

  test('theme prop update changes selected items', async () => {
    const yellowTheme = visualBrowserProxy.getYellowTheme();
    appearanceMenu.theme = yellowTheme;
    await microtasksFinished();

    const selectedItems = appearanceMenu.$.menu.menuGroups[1]!.items.filter(
        item => item.selected);
    assertEquals(1, selectedItems.length, 'selected');
    assertEquals(yellowTheme, selectedItems[0]!.data, 'data');
  });

  test('on theme change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    appearanceMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.THEME,
        {detail: {data: visualBrowserProxy.getDarkTheme()}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test('restores saved color option', async () => {
    const color = visualBrowserProxy.getYellowTheme();
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertNotEquals(color, startingSelected?.data);

    appearanceMenu.theme = color;
    await microtasksFinished();

    const newSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(color, newSelected?.data);
    assertNotEquals(startingSelected?.data, newSelected?.data);
  });

  test('does nothing if saved color is the same', async () => {
    const startingSelected =
        appearanceMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);

    appearanceMenu.theme = 0;
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

  test('hides theme submenu when forced-colors is active', async () => {
    stubAnimationFrame();

    // When forced-colors is disabled (default), the theme submenu is visible.
    assertEquals(2, appearanceMenu.$.menu.menuGroups.length);
    assertTrue(appearanceMenu.$.menu.menuGroups.some(
        group => group.eventName === ToolbarEvent.THEME));

    // When forced-colors is enabled, the theme submenu is hidden.
    window.matchMedia = (query: string) => ({
      matches: query === '(forced-colors: active)',
    } as MediaQueryList);

    appearanceMenu.open(document.body);
    await microtasksFinished();

    assertEquals(1, appearanceMenu.$.menu.menuGroups.length);
    assertEquals(
        ToolbarEvent.PRESENTATION_CHANGE,
        appearanceMenu.$.menu.menuGroups[0]!.eventName);
    assertFalse(appearanceMenu.$.menu.menuGroups.some(
        group => group.eventName === ToolbarEvent.THEME));

    // When forced-colors is disabled again, the theme submenu is visible again.
    window.matchMedia = () => ({
      matches: false,
    } as MediaQueryList);

    appearanceMenu.open(document.body);
    await microtasksFinished();

    assertEquals(2, appearanceMenu.$.menu.menuGroups.length);
    assertTrue(appearanceMenu.$.menu.menuGroups.some(
        group => group.eventName === ToolbarEvent.THEME));
  });
});
