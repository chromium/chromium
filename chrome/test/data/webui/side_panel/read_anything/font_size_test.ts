// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('FontSize', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let increaseButton: CrIconButtonElement|null;
  let decreaseButton: CrIconButtonElement|null;
  let fontSizeEmitted: boolean;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    fontSizeEmitted = false;
    document.addEventListener(
        ToolbarEvent.FONT_SIZE, () => fontSizeEmitted = true);
  });

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    return microtasksFinished();
  }

  suite('with read aloud', () => {
    setup(async () => {
      chrome.readingMode.isReadAloudEnabled = true;
      await createToolbar();

      menuButton =
          toolbar.shadowRoot.querySelector<CrIconButtonElement>('#font-size');
    });

    test('is dropdown menu', async () => {
      stubAnimationFrame();

      menuButton!.click();
      await microtasksFinished();

      assertTrue(toolbar.$.fontSizeMenu.get().open);
    });

    test('increase clicked increases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;

      toolbar.$.fontSizeMenu.get()
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();

      assertGT(chrome.readingMode.fontSize, startingFontSize);
      assertTrue(fontSizeEmitted);
    });

    test('decrease clicked decreases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;

      toolbar.$.fontSizeMenu.get()
          .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();

      assertGT(startingFontSize, chrome.readingMode.fontSize);
      assertTrue(fontSizeEmitted);
    });

    test('reset clicked returns font size to starting size', () => {
      const startingFontSize = chrome.readingMode.fontSize;

      toolbar.$.fontSizeMenu.get()
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
      toolbar.$.fontSizeMenu.get()
          .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
      assertGT(chrome.readingMode.fontSize, startingFontSize);

      toolbar.$.fontSizeMenu.get()
          .querySelector<CrIconButtonElement>('#font-size-reset')!.click();
      assertEquals(startingFontSize, chrome.readingMode.fontSize);
      assertTrue(fontSizeEmitted);
    });
  });

  suite('without read aloud', () => {
    setup(async () => {
      chrome.readingMode.isReadAloudEnabled = false;
      await createToolbar();

      menuButton =
          toolbar.shadowRoot.querySelector<CrIconButtonElement>('#font-size');
      increaseButton = toolbar.shadowRoot.querySelector<CrIconButtonElement>(
          '#font-size-increase-old');
      decreaseButton = toolbar.shadowRoot.querySelector<CrIconButtonElement>(
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
      assertTrue(fontSizeEmitted);
    });

    test('decrease clicked decreases container font size', () => {
      const startingFontSize = chrome.readingMode.fontSize;
      decreaseButton!.click();
      assertGT(startingFontSize, chrome.readingMode.fontSize);
      assertTrue(fontSizeEmitted);
    });
  });
});
