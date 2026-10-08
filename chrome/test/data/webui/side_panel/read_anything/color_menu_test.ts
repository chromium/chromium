// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {ColorMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('ColorMenuElement', () => {
  let colorMenu: ColorMenuElement;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

    colorMenu = document.createElement('color-menu');
    document.body.appendChild(colorMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(colorMenu);
  });

  test('theme change closes all menus', async () => {
    const closePromise = eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    colorMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.THEME,
        {detail: {data: visualBrowserProxy.getBlueTheme()}}));
    await closePromise;
  });

  test('restores saved color option', async () => {
    const color = visualBrowserProxy.getYellowTheme();
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;
    assertNotEquals(color, startingIndex);

    colorMenu.theme = color;
    await microtasksFinished();

    assertNotEquals(startingIndex, colorMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved color is the same', async () => {
    const startingIndex = colorMenu.$.menu.currentSelectedIndex;

    colorMenu.theme = 0;
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
