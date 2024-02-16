// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('FontSize', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let increaseButton: CrIconButtonElement|null;
  let decreaseButton: CrIconButtonElement|null;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  /**
   * Suppresses harmless ResizeObserver errors due to a browser bug.
   * yaqs/2300708289911980032
   */
  function suppressInnocuousErrors() {
    const onerror = window.onerror;
    window.onerror = (message, url, lineNumber, column, error) => {
      if ([
            'ResizeObserver loop limit exceeded',
            'ResizeObserver loop completed with undelivered notifications.',
          ].includes(message.toString())) {
        console.info('Suppressed ResizeObserver error: ', message);
        return;
      }
      if (onerror) {
        onerror.apply(window, [message, url, lineNumber, column, error]);
      }
    };
  }

  function createToolbar(): void {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
  }

  suite('with read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      createToolbar();

      menuButton =
          toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#font-size');
    });

    test('is dropdown menu', () => {
      menuButton!.click();
      assertTrue(toolbar.$.fontSizeMenu.open);
    });

    test('increase clicked increases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      menuButton!.click();

      toolbar.$.fontSizeMenu
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();

      assertGT(chrome.readingMode.fontSize, startingFontSize);
    });

    test('decrease clicked decreases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      menuButton!.click();

      toolbar.$.fontSizeMenu
          .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();

      assertGT(startingFontSize, chrome.readingMode.fontSize);
    });

    test('reset clicked returns font size to starting size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      menuButton!.click();

      toolbar.$.fontSizeMenu
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
      toolbar.$.fontSizeMenu
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
      assertGT(chrome.readingMode.fontSize, startingFontSize);

      toolbar.$.fontSizeMenu
          .querySelector<CrIconButtonElement>('#font-size-reset')!.click();
      assertEquals(startingFontSize, chrome.readingMode.fontSize);
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      createToolbar();

      menuButton =
          toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#font-size');
      increaseButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
          '#font-size-increase-old');
      decreaseButton = toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
          '#font-size-decrease-old');
    });

    test('is two buttons', () => {
      assertTrue(!!increaseButton);
      assertTrue(!!decreaseButton);
      assertFalse(!!menuButton);
    });

    test('increase clicked increases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      increaseButton!.click();
      assertGT(chrome.readingMode.fontSize, startingFontSize);
    });

    test('decrease clicked decreases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      decreaseButton!.click();
      assertGT(startingFontSize, chrome.readingMode.fontSize);
    });
  });
});
