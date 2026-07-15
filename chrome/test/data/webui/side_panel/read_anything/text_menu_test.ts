// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {TextMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('TextMenuElement', () => {
  let textMenu: TextMenuElement;
  let metrics: TestMetricsBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    readingMode.supportedFonts = ['Poppins', 'Sans-serif', 'Serif'];
    readingMode.fontName = 'Poppins';
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    textMenu = document.createElement('text-menu');
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
        textMenu.settingsPrefs = {
          ...textMenu.settingsPrefs,
          font: newFont,
        };
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected font count');
        assertEquals(newFont, selectedItems[0]!.data, 'selected font data');
      });

  test(
      'on font change invokes reading mode callback and logs metrics',
      async () => {
        let calledFont = '';
        chrome.readingMode.onFontChange = (val: string) => calledFont = val;
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newFont = 'Serif';
        textMenu.$.menu.dispatchEvent(
            new CustomEvent(ToolbarEvent.FONT, {detail: {data: newFont}}));
        await microtasksFinished();

        assertEquals(newFont, calledFont);
        assertEquals(
            ReadAnythingSettingsChange.FONT_CHANGE,
            await metrics.whenCalled('recordTextSettingsChange'));
        assertEquals(1, metrics.getCallCount('recordTextSettingsChange'));
        assertEquals(0, closeAllMenusCount);
      });

  test(
      'updating line spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const looseLineSpacing = chrome.readingMode.looseLineSpacing;
        textMenu.settingsPrefs = {
          ...textMenu.settingsPrefs,
          lineSpacing: looseLineSpacing,
        };
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[1]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected spacing count');
        assertEquals(
            looseLineSpacing, selectedItems[0]!.data, 'selected spacing data');
      });

  test(
      'on line spacing change invokes reading mode callback and logs metrics',
      async () => {
        let calledSpacing = -1;
        chrome.readingMode.onLineSpacingChange = (val: number) =>
            calledSpacing = val;
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newSpacing = chrome.readingMode.looseLineSpacing;
        textMenu.$.menu.dispatchEvent(new CustomEvent(
            ToolbarEvent.LINE_SPACING, {detail: {data: newSpacing}}));
        await microtasksFinished();

        assertEquals(newSpacing, calledSpacing);
        assertEquals(
            ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
            await metrics.whenCalled('recordTextSettingsChange'));
        assertEquals(1, metrics.getCallCount('recordTextSettingsChange'));
        assertEquals(0, closeAllMenusCount);
      });

  test(
      'updating letter spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const wideLetterSpacing = chrome.readingMode.wideLetterSpacing;
        textMenu.settingsPrefs = {
          ...textMenu.settingsPrefs,
          letterSpacing: wideLetterSpacing,
        };
        await microtasksFinished();

        const selectedItems =
            textMenu.$.menu.menuGroups[2]!.items.filter(item => item.selected);
        assertEquals(1, selectedItems.length, 'selected letter spacing count');
        assertEquals(
            wideLetterSpacing, selectedItems[0]!.data,
            'selected letter spacing data');
      });

  test(
      'on letter spacing change invokes reading mode callback and logs metrics',
      async () => {
        let calledSpacing = -1;
        chrome.readingMode.onLetterSpacingChange = (val: number) =>
            calledSpacing = val;
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newSpacing = chrome.readingMode.wideLetterSpacing;
        textMenu.$.menu.dispatchEvent(new CustomEvent(
            ToolbarEvent.LETTER_SPACING, {detail: {data: newSpacing}}));
        await microtasksFinished();

        assertEquals(newSpacing, calledSpacing);
        assertEquals(
            ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
            await metrics.whenCalled('recordTextSettingsChange'));
        assertEquals(1, metrics.getCallCount('recordTextSettingsChange'));
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
    const looseSpacing = chrome.readingMode.looseLineSpacing;
    const startingSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertNotEquals(looseSpacing, startingSelected?.data);

    textMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      lineSpacing: looseSpacing,
    };
    await microtasksFinished();

    const newSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);
    assertEquals(looseSpacing, newSelected?.data);
    assertNotEquals(startingSelected?.data, newSelected?.data);
  });

  test('does nothing if saved line spacing is the same', async () => {
    const startingSelected =
        textMenu.$.menu.menuGroups[1]!.items.find(item => item.selected);

    textMenu.settingsPrefs = {
      ...textMenu.settingsPrefs,
      lineSpacing: startingSelected?.data as number,
    };
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
