// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('FontSize', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let fontSizeEmitted: boolean;


  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    fontSizeEmitted = false;
    document.addEventListener(
        ToolbarEvent.FONT_SIZE, () => fontSizeEmitted = true);

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();

    menuButton =
        toolbar.shadowRoot.querySelector<CrIconButtonElement>('#font-size');
    assertTrue(!!menuButton);
  });

  // Returns the block used for aria announcement.
  // blockId is the css id of the announcement block.
  function getAnnouncementBlock(blockId: string): HTMLElement {
    const announcement_div =
        toolbar.$.toolbarContainer.querySelector<HTMLElement>(blockId);
    assertTrue(announcement_div !== null);
    return announcement_div;
  }

  function getAnnouncementBlockText(blockId: string): string|null|undefined {
    return getAnnouncementBlock(blockId).querySelector('p')?.textContent;
  }

  function getAnnouncementBlockNumParagraphs(blockId: string): number {
    return getAnnouncementBlock(blockId).querySelectorAll('p').length;
  }

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

  test('increase clicked changes aria-live region', () => {
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
    assertEquals(
        'Font size increased', getAnnouncementBlockText('#size-announce'));
  });


  test('decrease clicked decreases container font size', () => {
    const startingFontSize = chrome.readingMode.fontSize;

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();

    assertGT(startingFontSize, chrome.readingMode.fontSize);
    assertTrue(fontSizeEmitted);
  });

  test('aria-live region is not visible', () => {
    let announceBlock = getAnnouncementBlock('#size-announce');
    let width = announceBlock.offsetWidth;
    let height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();

    announceBlock = getAnnouncementBlock('#size-announce');
    assertEquals(1, announceBlock.querySelectorAll('p').length);
    width = announceBlock.offsetWidth;
    height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);
  });

  test('font size clicked keeps adding to aria-live region', () => {
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(2, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(3, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(4, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(5, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    // After 6 calls to increase size, the div will be cleared.
    assertEquals(0, getAnnouncementBlockNumParagraphs('#size-announce'));

    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));
  });

  test(' decrease clicked changes aria-live region', () => {
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
    assertEquals(
        toolbar.$.toolbarContainer.querySelector('#size-announce')
            ?.querySelector('p')
            ?.textContent,
        'Font size decreased');
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
