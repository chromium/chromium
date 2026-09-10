// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {TextMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('TextMenuElement', () => {
  let textMenu: TextMenuElement;
  let metrics: TestMetricsBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;
    visualBrowserProxy.supportedFonts = ['Poppins', 'Sans-serif', 'Serif'];
    visualBrowserProxy.fontName = 'Poppins';
    metrics = result.metrics;

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
        visualBrowserProxy.fontName = newFont;
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
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newFont = 'Serif';
        textMenu.$.menu.dispatchEvent(
            new CustomEvent(ToolbarEvent.FONT, {detail: {data: newFont}}));
        await microtasksFinished();

        assertEquals(1, visualBrowserProxy.getCallCount('onFontChange'));
        assertEquals(newFont, visualBrowserProxy.getArgs('onFontChange')[0]);
        assertEquals(
            ReadAnythingSettingsChange.FONT_CHANGE,
            await metrics.whenCalled('recordTextSettingsChange'));
        assertEquals(1, metrics.getCallCount('recordTextSettingsChange'));
        assertEquals(0, closeAllMenusCount);
      });

  test(
      'updating line spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const looseLineSpacing = visualBrowserProxy.looseLineSpacing;
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
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newSpacing = visualBrowserProxy.looseLineSpacing;
        textMenu.$.menu.dispatchEvent(new CustomEvent(
            ToolbarEvent.LINE_SPACING, {detail: {data: newSpacing}}));
        await microtasksFinished();

        assertEquals(1, visualBrowserProxy.getCallCount('onLineSpacingChange'));
        assertEquals(
            newSpacing, visualBrowserProxy.getArgs('onLineSpacingChange')[0]);
        assertEquals(
            ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
            await metrics.whenCalled('recordTextSettingsChange'));
        assertEquals(1, metrics.getCallCount('recordTextSettingsChange'));
        assertEquals(0, closeAllMenusCount);
      });

  test(
      'updating letter spacing preference property renders checkmark on the selected spacing item',
      async () => {
        const wideLetterSpacing = visualBrowserProxy.wideLetterSpacing;
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
        let closeAllMenusCount = 0;
        document.addEventListener(
            ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

        const newSpacing = visualBrowserProxy.wideLetterSpacing;
        textMenu.$.menu.dispatchEvent(new CustomEvent(
            ToolbarEvent.LETTER_SPACING, {detail: {data: newSpacing}}));
        await microtasksFinished();

        assertEquals(
            1, visualBrowserProxy.getCallCount('onLetterSpacingChange'));
        assertEquals(
            newSpacing, visualBrowserProxy.getArgs('onLetterSpacingChange')[0]);
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
    const looseSpacing = visualBrowserProxy.looseLineSpacing;
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
