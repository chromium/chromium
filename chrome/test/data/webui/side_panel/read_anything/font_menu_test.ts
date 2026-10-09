// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {FontMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, getItemsInMenu, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('FontMenu', () => {
  let fontMenu: FontMenuElement;
  let fontMenuOptions: HTMLButtonElement[];
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;
    fontMenu = document.createElement('font-menu');
    document.body.appendChild(fontMenu);
  });

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        expected.trim().toLowerCase().replaceAll('"', ''),
        actual.trim().toLowerCase().replaceAll('"', ''));
  }

  async function updateFonts(supportedFonts: string[]): Promise<void> {
    visualBrowserProxy.supportedFonts = supportedFonts;
    fontMenu.pageLanguage = 'hi' + supportedFonts.length;
    await microtasksFinished();
    fontMenuOptions = getItemsInMenu(fontMenu.$.menu.$.lazyMenu);
  }

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(fontMenu);
  });

  test('updates fonts on page language change', async () => {
    visualBrowserProxy.supportedFonts =
        ['font 1', 'font 2', 'font 3', 'font 4'];
    fontMenu.pageLanguage = 'hi';
    await microtasksFinished();
    assertEquals(4, getItemsInMenu(fontMenu.$.menu.$.lazyMenu).length);

    visualBrowserProxy.supportedFonts = ['font 1', 'font 2'];
    fontMenu.pageLanguage = 'jp';
    await microtasksFinished();
    assertEquals(2, getItemsInMenu(fontMenu.$.menu.$.lazyMenu).length);
  });

  test('updates font titles on fonts loaded', async () => {
    visualBrowserProxy.supportedFonts = ['font 1', 'font 2', 'font 3'];
    fontMenuOptions = getItemsInMenu(fontMenu.$.menu.$.lazyMenu);
    assertTrue(fontMenuOptions.every(
        option => option.innerText.includes('(loading)')));

    fontMenu.areFontsLoaded = true;
    await microtasksFinished();

    assertFalse(
        fontMenuOptions.some(option => option.innerText.includes('(loading)')));
  });

  test('updates selection on font change', async () => {
    visualBrowserProxy.supportedFonts = ['font 1', 'font 2', 'font 3'];
    fontMenu.font = 'font 1';
    fontMenu.areFontsLoaded = true;
    await microtasksFinished();
    assertEquals(0, fontMenu.$.menu.currentSelectedIndex);

    fontMenu.font = 'font 2';
    await microtasksFinished();

    assertEquals(1, fontMenu.$.menu.currentSelectedIndex);
  });

  test('uses the first font if font not available', async () => {
    // Set the current font to one that will be removed
    const defaultFont = 'EB Garamond';
    const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
    visualBrowserProxy.fontName = defaultFont;
    fontMenu.font = defaultFont;
    await updateFonts(fonts.concat(defaultFont));

    // Update the fonts to exclude the previously chosen font
    await updateFonts(fonts);

    const checkMarks =
        fontMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.check-mark-showing-true');
    const hiddenCheckMarks =
        fontMenu.$.menu.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            '.check-mark-showing-false');
    assertEquals(1, checkMarks.length);
    assertEquals(2, hiddenCheckMarks.length);
    assertEquals(0, fontMenu.$.menu.currentSelectedIndex);
    // Avoid overriding the user default font
    assertEquals(defaultFont, visualBrowserProxy.getFontName());
  });

  test('each font option is styled with the font that it is', async () => {
    await updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
    fontMenu.areFontsLoaded = true;
    await microtasksFinished();
    fontMenuOptions.forEach(option => {
      assertFontsEqual(option.style.fontFamily, option.innerText);
    });
  });

  test('font change closes all menus', async () => {
    const closePromise = eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    fontMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.FONT, {detail: {data: 'Poppins'}}));
    await closePromise;
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    fontMenu.open(document.body);
    assertTrue(fontMenu.$.menu.$.lazyMenu.get().open);
    fontMenu.close();
    assertFalse(fontMenu.$.menu.$.lazyMenu.get().open);
  });
});
