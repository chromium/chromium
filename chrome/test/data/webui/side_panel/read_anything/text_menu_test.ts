// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {TextMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('TextMenuElement', () => {
  let textMenu: TextMenuElement;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;
    visualBrowserProxy.supportedFonts = ['Poppins', 'Sans-serif', 'Serif'];

    textMenu = document.createElement('text-menu');
    textMenu.font = 'Poppins';
    textMenu.areFontsLoaded = true;
    document.body.appendChild(textMenu);
  });

  test('dropdown renders checkmark elements for all menu groups', () => {
    assertCheckMarksForDropdown(textMenu);
  });

  test(
      'updating font preference property renders checkmark on the selected font item',
      async () => {
        const newFont = 'Serif';
        textMenu.font = newFont;
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected font count');
        assertEquals(newFont, selectedItems[0]!.data, 'selected font data');
      });

  test('on font change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    textMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.FONT, {detail: {data: 'Serif'}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test(
      'updating line spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const looseLineSpacing = visualBrowserProxy.looseLineSpacing;
        textMenu.lineSpacing = looseLineSpacing;
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected spacing count');
        assertEquals(
            looseLineSpacing, selectedItems[0]!.data, 'selected spacing data');
      });

  test('on line spacing change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    textMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LINE_SPACING,
        {detail: {data: visualBrowserProxy.looseLineSpacing}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test(
      'updating letter spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const wideLetterSpacing = visualBrowserProxy.wideLetterSpacing;
        textMenu.letterSpacing = wideLetterSpacing;
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[2]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected letter spacing count');
        assertEquals(
            wideLetterSpacing, selectedItems[0]!.data,
            'selected letter spacing data');
      });

  test('on letter spacing change does not close menus', async () => {
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    textMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING,
        {detail: {data: visualBrowserProxy.wideLetterSpacing}}));
    await microtasksFinished();

    assertEquals(0, closeAllMenusCount);
  });

  test(
      'font option titles show loading string when fonts not loaded',
      async () => {
        textMenu.areFontsLoaded = false;
        await microtasksFinished();

        const fontItems = textMenu.$.menu.menuGroups[0]!.items;
        assertTrue(fontItems.length > 0);
        fontItems.forEach(item => {
          assertTrue(
              item.title.includes(textMenu.i18n('readingModeFontLoadingText')));
        });
      });

  test(
      'font option titles do not show loading string when fonts loaded',
      async () => {
        textMenu.areFontsLoaded = true;
        await microtasksFinished();

        const fontItems = textMenu.$.menu.menuGroups[0]!.items;
        assertTrue(fontItems.length > 0);
        fontItems.forEach(item => {
          assertFalse(
              item.title.includes(textMenu.i18n('readingModeFontLoadingText')));
        });
      });

  test('restores saved line spacing option', async () => {
    const looseSpacing = visualBrowserProxy.looseLineSpacing;
    const startingSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertNotEquals(looseSpacing, startingSelected?.data);

    textMenu.lineSpacing = looseSpacing;
    await microtasksFinished();

    const newSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(looseSpacing, newSelected?.data);
    assertNotEquals(startingSelected?.data, newSelected?.data);
  });

  test('does nothing if saved line spacing is the same', async () => {
    const startingSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);

    textMenu.lineSpacing = startingSelected?.data as number;
    await microtasksFinished();

    const newSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(startingSelected?.data, newSelected?.data);
  });

  test('open and close methods control menu visibility', () => {
    stubAnimationFrame();
    textMenu.open(document.body);
    assertTrue(textMenu.$.menu.$.lazyMenu.get().open);
    textMenu.close();
    assertFalse(textMenu.$.menu.$.lazyMenu.get().open);
  });
});
