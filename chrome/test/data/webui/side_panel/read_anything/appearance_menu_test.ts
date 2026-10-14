// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppearanceMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';
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

  test('theme prop update changes selected items', async () => {
    const yellowTheme = visualBrowserProxy.getYellowTheme();
    appearanceMenu.theme = yellowTheme;
    await microtasksFinished();

    assertEquals(
        yellowTheme, appearanceMenu.$.menu.menuGroups[1]!.selectedValue);
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
    assertNotEquals(color, appearanceMenu.$.menu.menuGroups[1]!.selectedValue);

    appearanceMenu.theme = color;
    await microtasksFinished();

    assertEquals(color, appearanceMenu.$.menu.menuGroups[1]!.selectedValue);
  });

  test('presentation prop update changes selected items', async () => {
    appearanceMenu.presentationState =
        visualBrowserProxy.getInImmersiveOverlayPresentationState();
    await microtasksFinished();

    assertEquals(
        visualBrowserProxy.getInImmersiveOverlayPresentationState(),
        appearanceMenu.$.menu.menuGroups[0]!.selectedValue);
  });

  test('on presentation change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    appearanceMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.PRESENTATION_CHANGE, {
          detail: {
            data: visualBrowserProxy.getInImmersiveOverlayPresentationState(),
          },
        }));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
    assertEquals(0, visualBrowserProxy.getCallCount('togglePresentation'));
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
