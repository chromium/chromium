// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {getItemsInMenu, stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('FontMenu', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let fontSelect: HTMLSelectElement|null;
  let fontEmitted: boolean;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.supportedFonts = [];
    fontEmitted = false;
    document.addEventListener(ToolbarEvent.FONT, () => fontEmitted = true);
  });

  function createToolbar(): void {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();
    menuButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#font');
    fontSelect =
        toolbar.shadowRoot!.querySelector<HTMLSelectElement>('#font-select');
  }

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        expected.trim().toLowerCase().replaceAll('"', ''),
        actual.trim().toLowerCase().replaceAll('"', ''));
  }

  suite('with read aloud', () => {
    let fontMenuOptions: HTMLButtonElement[];

    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      createToolbar();
    });

    function updateFonts(supportedFonts: string[]): void {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
      fontMenuOptions = getItemsInMenu(toolbar.$.fontMenu);
    }

    test('is dropdown menu', () => {
      stubAnimationFrame();

      menuButton!.click();
      flush();

      assertTrue(toolbar.$.fontMenu.get().open);
    });

    test('shows only supported fonts', () => {
      updateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(fontMenuOptions.length, 4);

      updateFonts(['font 1']);
      assertEquals(1, fontMenuOptions.length);

      // initial-count in the dom-repeat for the fonts menu limits the
      // size of the font menu, so adding more than 8 fonts is difficult to
      // test. If more than 8 fonts are added on the actual menu, we can
      // increase the initial-count.
      updateFonts([
        'font 1',
        'font 2',
        'font 3',
        'font 4',
        'font 5',
        'font 6',
        'font 7',
        'font 8',
      ]);
      assertEquals(8, fontMenuOptions.length);
    });

    test('uses the first font if font not available', () => {
      // Set the current font to one that will be removed
      const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
      chrome.readingMode.fontName = 'EB Garamond';
      updateFonts(fonts.concat(chrome.readingMode.fontName));

      // Update the fonts to exclude the previously chosen font
      updateFonts(fonts);

      const checkMarks = toolbar.$.fontMenu.get().querySelectorAll<HTMLElement>(
          '.check-mark-hidden-false');
      const hiddenCheckMarks =
          toolbar.$.fontMenu.get().querySelectorAll<HTMLElement>(
              '.check-mark-hidden-true');
      assertEquals(1, checkMarks.length);
      assertEquals(2, hiddenCheckMarks.length);
      assertEquals(fonts[0], chrome.readingMode.fontName);
      assertEquals(fonts[0], toolbar.style.fontFamily);
    });

    test('each font option is styled with the font that it is', () => {
      updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
      toolbar.setFontsLoaded();
      fontMenuOptions.forEach(option => {
        assertFontsEqual(option.style.fontFamily, option.innerText);
      });
    });

    test('each font option is loading', () => {
      updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
      fontMenuOptions.forEach(option => {
        assertFontsEqual(
            option.style.fontFamily + '\u00A0(loading)', option.innerText);
      });
    });

    suite('on font option clicked', () => {
      setup(() => {
        updateFonts(['Serif', 'Poppins', 'STIX Two Text']);
        menuButton!.click();
      });

      test('propagates font', () => {
        fontMenuOptions.forEach((option, index) => {
          fontEmitted = false;
          option.click();
          const expectedFont = chrome.readingMode.supportedFonts[index]!;
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertTrue(fontEmitted);
        });
      });

      test('updates toolbar font', () => {
        fontMenuOptions.forEach((option, index) => {
          option.click();
          assertFontsEqual(
              toolbar.style.fontFamily,
              chrome.readingMode.supportedFonts[index]!);
        });
      });

      test('closes menu', () => {
        fontMenuOptions.forEach((option) => {
          option.click();
          assertFalse(toolbar.$.fontMenu.get().open);
        });
      });
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      createToolbar();
    });

    function updateFonts(supportedFonts: string[]): void {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
      flush();
    }

    test('is select menu', () => {
      assertTrue(!!fontSelect);
      assertFalse(!!menuButton);
    });

    test('shows only supported fonts', () => {
      updateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(
          chrome.readingMode.supportedFonts.length, fontSelect!.options.length);
    });

    test('uses the first font if font not available', () => {
      // Set the current font to one that will be removed
      const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
      chrome.readingMode.fontName = 'EB Garamond';
      updateFonts(fonts.concat(chrome.readingMode.fontName));

      // Update the fonts to exclude the previously chosen font
      updateFonts(fonts);

      assertEquals(0, fontSelect!.selectedIndex);
      assertEquals(fonts[0], chrome.readingMode.fontName);
      assertEquals(fonts[0], toolbar.style.fontFamily);
    });

    suite('on font option clicked', () => {
      setup(() => {
        updateFonts(['Serif', 'Poppins', 'STIX Two Text']);
      });

      test('propagates font', () => {
        chrome.readingMode.supportedFonts.forEach((expectedFont, index) => {
          fontEmitted = false;
          fontSelect!.selectedIndex = index;
          fontSelect!.dispatchEvent(new Event('change'));
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertTrue(fontEmitted);
        });
      });

      test('updates toolbar font', () => {
        chrome.readingMode.supportedFonts.forEach((supportedFont, index) => {
          fontSelect!.selectedIndex = index;
          fontSelect!.dispatchEvent(new Event('change'));
          assertFontsEqual(toolbar.style.fontFamily, supportedFont);
        });
      });
    });
  });
});
