// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {FONT_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('FontMenu', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let fontSelect: HTMLSelectElement|null;
  let fontEmitted: string;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.supportedFonts = [];
    fontEmitted = '';
    document.addEventListener(FONT_EVENT, event => {
      fontEmitted = (event as CustomEvent).detail.fontName;
    });
  });

  function createToolbar(): void {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    menuButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#font');
    fontSelect =
        toolbar.shadowRoot!.querySelector<HTMLSelectElement>('#font-select');
  }

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        actual.trim().toLowerCase().replaceAll('"', ''),
        expected.trim().toLowerCase().replaceAll('"', ''));
  }

  suite('with read aloud', () => {
    let fontMenuOptions: HTMLButtonElement[];

    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      createToolbar();
    });

    function udpateFonts(supportedFonts: string[]): void {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
      fontMenuOptions =
          Array.from(toolbar.$.fontMenu.querySelectorAll<HTMLButtonElement>(
              '.dropdown-item'));
    }

    test('is dropdown menu', () => {
      menuButton!.click();
      assertTrue(toolbar.$.fontMenu.open);
    });

    test('shows only supported fonts', () => {
      udpateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(fontMenuOptions.length, 4);

      udpateFonts(['font 1']);
      assertEquals(fontMenuOptions.length, 1);

      udpateFonts([
        'font 1',
        'font 2',
        'font 3',
        'font 4',
        'font 5',
        'font 6',
        'font 7',
        'font 8',
        'font 9',
        'font 10',
      ]);
      assertEquals(fontMenuOptions.length, 10);
    });

    test('each font option is styled with the font that it is', () => {
      udpateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
      fontMenuOptions.forEach(option => {
        assertFontsEqual(option.style.fontFamily, option.innerText);
      });
    });

    suite('on font option clicked', () => {
      setup(() => {
        udpateFonts(['Serif', 'Poppins', 'STIX Two Text']);
        menuButton!.click();
      });

      test('propagates font', () => {
        fontMenuOptions.forEach((option, index) => {
          option.click();
          const expectedFont = chrome.readingMode.supportedFonts[index]!;
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertFontsEqual(fontEmitted, expectedFont);
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
          assertFalse(toolbar.$.fontMenu.open);
        });
      });
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      createToolbar();
    });

    function udpateFonts(supportedFonts: string[]): void {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
    }

    test('is select menu', () => {
      assertTrue(!!fontSelect);
      assertFalse(!!menuButton);
    });

    test('shows only supported fonts', () => {
      udpateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(
          chrome.readingMode.supportedFonts.length, fontSelect!.options.length);
    });

    suite('on font option clicked', () => {
      setup(() => {
        udpateFonts(['Serif', 'Poppins', 'STIX Two Text']);
      });

      test('propagates font', () => {
        chrome.readingMode.supportedFonts.forEach((expectedFont, index) => {
          fontSelect!.selectedIndex = index;
          fontSelect!.dispatchEvent(new Event('change'));
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertFontsEqual(fontEmitted, expectedFont);
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
