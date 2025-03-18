// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {getItemsInMenu, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('FontMenu', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let fontSelect: HTMLSelectElement|null;
  let fontEmitted: boolean;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.supportedFonts = [];
    fontEmitted = false;
    document.addEventListener(ToolbarEvent.FONT, () => fontEmitted = true);
  });

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    menuButton = toolbar.shadowRoot.querySelector<CrIconButtonElement>('#font');
    fontSelect =
        toolbar.shadowRoot.querySelector<HTMLSelectElement>('#font-select');
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
      return createToolbar();
    });

    async function updateFonts(supportedFonts: string[]): Promise<void> {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
      await microtasksFinished();
      fontMenuOptions = getItemsInMenu(toolbar.$.fontMenu);
    }

    test('is dropdown menu', async () => {
      stubAnimationFrame();

      menuButton!.click();
      await microtasksFinished();

      assertTrue(toolbar.$.fontMenu.get().open);
    });

    test('shows only supported fonts', async () => {
      await updateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(fontMenuOptions.length, 4);

      await updateFonts(['font 1']);
      assertEquals(1, fontMenuOptions.length);

      await updateFonts([
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

    test('uses the first font if font not available', async () => {
      // Set the current font to one that will be removed
      const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
      chrome.readingMode.fontName = 'EB Garamond';
      await updateFonts(fonts.concat(chrome.readingMode.fontName));

      // Update the fonts to exclude the previously chosen font
      await updateFonts(fonts);

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

    test('each font option is styled with the font that it is', async () => {
      await updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
      toolbar.setFontsLoaded();
      await microtasksFinished();
      fontMenuOptions.forEach(option => {
        assertFontsEqual(option.style.fontFamily, option.innerText);
      });
    });

    test('each font option is loading', async () => {
      await updateFonts(['Serif', 'Andika', 'Poppins', 'STIX Two Text']);
      fontMenuOptions.forEach(option => {
        assertFontsEqual(
            option.style.fontFamily + '\u00A0(loading)', option.innerText);
      });
    });

    suite('on font option clicked', () => {
      setup(async () => {
        await updateFonts(['Serif', 'Poppins', 'STIX Two Text']);
        menuButton!.click();
      });

      test('propagates font', async () => {
        for (let i = 0; i < fontMenuOptions.length; i++) {
          const option = fontMenuOptions[i]!;
          fontEmitted = false;
          option.click();
          await microtasksFinished();
          const expectedFont = chrome.readingMode.supportedFonts[i]!;
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertTrue(fontEmitted);
        }
      });

      test('updates toolbar font', async () => {
        for (let i = 0; i < fontMenuOptions.length; i++) {
          const option = fontMenuOptions[i]!;
          option.click();
          await microtasksFinished();
          assertFontsEqual(
              toolbar.style.fontFamily, chrome.readingMode.supportedFonts[i]!);
        }
      });

      test('closes menu', async () => {
        for (const option of fontMenuOptions) {
          option.click();
          await microtasksFinished();
          assertFalse(toolbar.$.fontMenu.get().open);
        }
      });
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      return createToolbar();
    });

    async function updateFonts(supportedFonts: string[]): Promise<void> {
      chrome.readingMode.supportedFonts = supportedFonts;
      toolbar.updateFonts();
      return microtasksFinished();
    }

    test('is select menu', () => {
      assertTrue(!!fontSelect);
      assertFalse(!!menuButton);
    });

    test('shows only supported fonts', async () => {
      await updateFonts(['font 1', 'font 2', 'font 3', 'font 4']);
      assertEquals(
          chrome.readingMode.supportedFonts.length, fontSelect!.options.length);
    });

    test('uses the first font if font not available', async () => {
      // Set the current font to one that will be removed
      const fonts = ['Andika', 'Poppins', 'STIX Two Text'];
      chrome.readingMode.fontName = 'EB Garamond';
      await updateFonts(fonts.concat(chrome.readingMode.fontName));

      // Update the fonts to exclude the previously chosen font
      await updateFonts(fonts);

      assertEquals(0, fontSelect!.selectedIndex);
      assertEquals(fonts[0], chrome.readingMode.fontName);
      assertEquals(fonts[0], toolbar.style.fontFamily);
    });

    suite('on font option clicked', () => {
      setup(() => {
        return updateFonts(['Serif', 'Poppins', 'STIX Two Text']);
      });

      test('propagates font', async () => {
        for (let i = 0; i < chrome.readingMode.supportedFonts.length; i++) {
          const expectedFont = chrome.readingMode.supportedFonts[i]!;
          fontEmitted = false;
          fontSelect!.selectedIndex = i;
          fontSelect!.dispatchEvent(new Event('change'));
          await microtasksFinished();
          assertFontsEqual(chrome.readingMode.fontName, expectedFont);
          assertTrue(fontEmitted);
        }
      });

      test('updates toolbar font', async () => {
        for (let i = 0; i < chrome.readingMode.supportedFonts.length; i++) {
          const supportedFont = chrome.readingMode.supportedFonts[i]!;
          fontSelect!.selectedIndex = i;
          fontSelect!.dispatchEvent(new Event('change'));
          await microtasksFinished();
          assertFontsEqual(toolbar.style.fontFamily, supportedFont);
        }
      });
    });
  });
});
