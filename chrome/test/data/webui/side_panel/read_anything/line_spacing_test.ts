// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {LineSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('LineSpacing', () => {
  let lineSpacingMenu: LineSpacingMenuElement;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

    lineSpacingMenu = document.createElement('line-spacing-menu');
    document.body.appendChild(lineSpacingMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(lineSpacingMenu);
  });

  test('spacing change closes all menus', async () => {
    const closePromise = eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    lineSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_SPACING,
        {detail: {data: visualBrowserProxy.getVeryLooseLineSpacing()}}));
    await closePromise;
  });

  test('restores saved spacing option', async () => {
    const spacing = visualBrowserProxy.getVeryLooseLineSpacing();
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    lineSpacingMenu.lineSpacing = spacing;
    await microtasksFinished();

    assertNotEquals(startingIndex, lineSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = lineSpacingMenu.$.menu.currentSelectedIndex;

    lineSpacingMenu.lineSpacing = 0;
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
