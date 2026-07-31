// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {TextMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, LineFocusMovement, LineFocusStyle, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
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
        chrome.readingMode.fontName = newFont;
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

  suite('With line focus enabled', () => {
    setup(async () => {
      metrics.reset();
      chrome.readingMode.isLineFocusEnabled = true;
      chrome.readingMode.isReadAnythingImprovedUiEnabled = true;
      textMenu.lineFocusEnabled = true;
      textMenu.settingsPrefs = {...textMenu.settingsPrefs};
      await microtasksFinished();
    });

    test('adds line focus menu groups to dropdown when on', () => {
      assertEquals(6, textMenu.$.menu.menuGroups.length);
    });

    test(
        'hides style and movement groups when lineFocusEnabled is false',
        async () => {
          textMenu.lineFocusEnabled = false;
          await microtasksFinished();
          assertEquals(4, textMenu.$.menu.menuGroups.length);
        });

    test('line focus style prop update changes selected items', async () => {
      const window = LineFocusStyle.MEDIUM_WINDOW;
      textMenu.lineFocusStyle = window;
      await microtasksFinished();
      let selectedItems =
          textMenu.$.menu.menuGroups[4]!.items.filter(item => item.selected);
      assertEquals(1, selectedItems.length, 'selected');
      assertEquals(window, selectedItems[0]!.data, 'data');

      const line = LineFocusStyle.UNDERLINE;
      textMenu.lineFocusStyle = line;
      await microtasksFinished();
      selectedItems =
          textMenu.$.menu.menuGroups[4]!.items.filter(item => item.selected);
      assertEquals(1, selectedItems.length, 'selected line');
      assertEquals(line, selectedItems[0]!.data, 'data line');
    });

    test(
        'line focus enabled prop update changes selected items and group count',
        async () => {
          textMenu.lineFocusEnabled = true;
          await microtasksFinished();
          let selectedItems = textMenu.$.menu.menuGroups[3]!.items.filter(
              item => item.selected);
          assertEquals(1, selectedItems.length, 'selected true');
          assertEquals(true, selectedItems[0]!.data);
          assertEquals(6, textMenu.$.menu.menuGroups.length);

          textMenu.lineFocusEnabled = false;
          await microtasksFinished();
          selectedItems = textMenu.$.menu.menuGroups[3]!.items.filter(
              item => item.selected);
          assertEquals(1, selectedItems.length, 'selected false');
          assertEquals(false, selectedItems[0]!.data);
          assertEquals(4, textMenu.$.menu.menuGroups.length);
        });

    test('line focus movement prop update changes selected items', async () => {
      const cursor = LineFocusMovement.CURSOR;
      textMenu.lineFocusMovement = cursor;
      await microtasksFinished();
      let selectedItems =
          textMenu.$.menu.menuGroups[5]!.items.filter(item => item.selected);
      assertEquals(1, selectedItems.length);
      assertEquals(cursor, selectedItems[0]!.data);

      const staticMovement = LineFocusMovement.STATIC;
      textMenu.lineFocusMovement = staticMovement;
      await microtasksFinished();
      selectedItems =
          textMenu.$.menu.menuGroups[5]!.items.filter(item => item.selected);
      assertEquals(1, selectedItems.length);
      assertEquals(staticMovement, selectedItems[0]!.data);
    });

    test('on line focus style change', async () => {
      let closeAllMenusCount = 0;
      document.addEventListener(
          ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

      textMenu.$.menu.dispatchEvent(new CustomEvent(
          ToolbarEvent.LINE_FOCUS_STYLE,
          {detail: {data: LineFocusStyle.LARGE_WINDOW}}));
      await microtasksFinished();

      assertEquals(
          ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE,
          await metrics.whenCalled('recordTextSettingsChange'));
      assertEquals(0, closeAllMenusCount);
    });

    test('on line focus toggle change', async () => {
      let closeAllMenusCount = 0;
      document.addEventListener(
          ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

      textMenu.$.menu.dispatchEvent(new CustomEvent(
          ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}}));
      await microtasksFinished();

      assertEquals(
          ReadAnythingSettingsChange.LINE_FOCUS_TOGGLE,
          await metrics.whenCalled('recordTextSettingsChange'));
      assertEquals(0, closeAllMenusCount);
    });

    test('on line focus movement change', async () => {
      let closeAllMenusCount = 0;
      document.addEventListener(
          ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

      textMenu.$.menu.dispatchEvent(new CustomEvent(
          ToolbarEvent.LINE_FOCUS_MOVEMENT,
          {detail: {data: LineFocusMovement.CURSOR}}));
      await microtasksFinished();

      assertEquals(
          ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE,
          await metrics.whenCalled('recordTextSettingsChange'));
      assertEquals(0, closeAllMenusCount);
    });

    test('can be closed programatically', () => {
      stubAnimationFrame();
      textMenu.open(document.body);
      assertTrue(textMenu.$.menu.$.lazyMenu.get().open);
      textMenu.close();
      assertFalse(textMenu.$.menu.$.lazyMenu.get().open);
    });

    test(
        'does not add line focus menu groups when isLineFocusEnabled is false',
        async () => {
          chrome.readingMode.isLineFocusEnabled = false;
          textMenu.settingsPrefs = {...textMenu.settingsPrefs};
          await microtasksFinished();

          assertEquals(3, textMenu.$.menu.menuGroups.length);
        });
  });
});

suite('TextMenuExpandableFonts', () => {
  let textMenu: TextMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    // Mock 5 supported fonts so truncation kicks in
    readingMode.supportedFonts = [
      'Poppins',
      'Sans-serif',
      'Serif',
      'Arial',
      'Comic Sans MS',
    ];
    // Default active font is the first one
    readingMode.fontName = 'Poppins';
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    textMenu = document.createElement('text-menu');
    textMenu.areFontsLoaded = true;
    document.body.appendChild(textMenu);
    await microtasksFinished();
    textMenu.open(document.body);
    await microtasksFinished();
  });

  test('initial view shows top 3 fonts plus expand button', () => {
    const fontGroupButtons =
        textMenu.shadowRoot.querySelector('grouped-action-menu')!.shadowRoot
            .querySelectorAll<HTMLButtonElement>(
                'button[data-group-index="0"]');

    // 3 fonts + 1 expand button = 4 buttons total
    assertEquals(4, fontGroupButtons.length);

    // Verify first 3 buttons have role="menuitemradio" and checkmark icon slots
    for (let i = 0; i < 3; i++) {
      assertEquals('menuitemradio', fontGroupButtons[i]!.getAttribute('role'));
      assertTrue(!!fontGroupButtons[i]!.querySelector('.check-mark'));
    }

    // Verify 4th button is the expand trigger with role="menuitem" and NO
    // checkmark slot
    const expandButton = fontGroupButtons[3]!;
    assertEquals('menuitem', expandButton.getAttribute('role'));
    assertNull(expandButton.getAttribute('aria-checked'));
    assertNull(expandButton.querySelector('.check-mark'));
  });

  test('expanding font menu reveals all supported fonts', async () => {
    // Set active font in settingsPrefs so an item is selected
    textMenu.settingsPrefs = {
      ...textMenu.settingsPrefs,
      font: 'Poppins',
    };
    await microtasksFinished();

    // Initial state: 3 fonts + 1 expand item = 4 items
    assertEquals(4, textMenu.$.menu.menuGroups[0]!.items.length);
    assertEquals(
        ToolbarEvent.EXPAND_FONTS_SENTINEL,
        textMenu.$.menu.menuGroups[0]!.items[3]!.data);

    // Expand the menu
    textMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.FONT,
        {detail: {data: ToolbarEvent.EXPAND_FONTS_SENTINEL}}));
    await microtasksFinished();

    // Verify all 5 fonts are now present
    assertEquals(5, textMenu.$.menu.menuGroups[0]!.items.length);

    // Verify checkmark selection is maintained (exactly 1 selected item)
    const selectedItems =
        textMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals('Poppins', selectedItems[0]!.data);
  });

  test('closing the menu resets it to unexpanded state', async () => {
    // Expand the menu
    textMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.FONT,
        {detail: {data: ToolbarEvent.EXPAND_FONTS_SENTINEL}}));
    await microtasksFinished();
    assertEquals(5, textMenu.$.menu.menuGroups[0]!.items.length);
    // Close the menu
    textMenu.close();
    await microtasksFinished();
    // Verify menu returned to unexpanded state (3 fonts + 1 expand = 4 items)
    assertEquals(4, textMenu.$.menu.menuGroups[0]!.items.length);
  });
});
