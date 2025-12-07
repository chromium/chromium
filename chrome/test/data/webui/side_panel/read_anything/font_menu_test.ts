// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {FontMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, getItemsInMenu, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('FontMenu', () => {
  let fontMenu: FontMenuElement;
  let fontMenuOptions: HTMLButtonElement[];
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.supportedFonts = [];
    metrics = mockMetrics();
    fontMenu = document.createElement('font-menu');
    document.body.appendChild(fontMenu);
  });

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        expected.trim().toLowerCase().replaceAll('"', ''),
        actual.trim().toLowerCase().replaceAll('"', ''));
  }

  async function updateFonts(supportedFonts: string[]): Promise<void> {
    chrome.readingMode.supportedFonts = supportedFonts;
    fontMenu.pageLanguage = 'hi' + supportedFonts.length;
    await microtasksFinished();
    fontMenuOptions = getItemsInMenu(fontMenu.$.menu.$.lazyMenu);
  }

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(fontMenu);
  });

  test('updates fonts on page language change', async () => {
    chrome.readingMode.supportedFonts =
        ['font 1', 'font 2', 'font 3', 'font 4'];
    fontMenu.pageLanguage = 'hi';
    await microtasksFinished();
    assertEquals(4, getItemsInMenu(fontMenu.$.menu.$.lazyMenu).length);

    chrome.readingMode.supportedFonts = ['font 1', 'font 2'];
    fontMenu.pageLanguage = 'jp';
    await microtasksFinished();
    assertEquals(2, getItemsInMenu(fontMenu.$.menu.$.lazyMenu).length);
  });

  test('updates font titles on fonts loaded', async () => {
    chrome.readingMode.supportedFonts = ['font 1', 'font 2', 'font 3'];
    fontMenuOptions = getItemsInMenu(fontMenu.$.menu.$.lazyMenu);
    assertTrue(fontMenuOptions.every(
        option => option.innerText.includes('(loading)')));

    fontMenu.areFontsLoaded = true;
    await microtasksFinished();

    assertFalse(
        fontMenuOptions.some(option => option.innerText.includes('(loading)')));
  });

  test('updates fonts when settings are restored', async () => {
    chrome.readingMode.supportedFonts = ['font 1', 'font 2', 'font 3'];
    chrome.readingMode.fontName = 'font 1';
    fontMenu.areFontsLoaded = true;
    await microtasksFinished();
    assertEquals(0, fontMenu.$.menu.currentSelectedIndex);

    chrome.readingMode.fontName = 'font 2';
    fontMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: 0,
      speechRate: 0,
      font: chrome.readingMode.fontName,
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertEquals(1, fontMenu.$.menu.currentSelectedIndex);
  });

  test('uses the first font if font not available', async () => {
    // Set the current font to one that will be removed
    const defaultFont = 'EB Garamond';
    const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
    chrome.readingMode.fontName = defaultFont;
    await updateFonts(fonts.concat(chrome.readingMode.fontName));

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
    assertEquals(defaultFont, chrome.readingMode.fontName);
  });

  test('each font option is styled with the font that it is', async () => {
    await updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
    fontMenu.areFontsLoaded = true;
    await microtasksFinished();
    fontMenuOptions.forEach(option => {
      assertFontsEqual(option.style.fontFamily, option.innerText);
    });
  });

  test('propagates font', async () => {
    const font1 = 'Times';
    fontMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.FONT, {detail: {data: font1}}));
    assertEquals(font1, chrome.readingMode.fontName);

    const font2 = 'Poppins';
    fontMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.FONT, {detail: {data: font2}}));
    assertEquals(font2, chrome.readingMode.fontName);

    const font3 = 'STIX Two Text';
    fontMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.FONT, {detail: {data: font3}}));
    assertEquals(font3, chrome.readingMode.fontName);

    assertEquals(
        ReadAnythingSettingsChange.FONT_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });
});
