// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('FontSize', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(async () => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

    toolbar = document.createElement('read-anything-toolbar');
    toolbar.fontSize = visualBrowserProxy.defaultFontSize;
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
        toolbar.$.fontSizeMenu.get().querySelector<HTMLElement>(blockId);
    assertTrue(announcement_div !== null);
    return announcement_div;
  }

  function getAnnouncementBlockText(blockId: string): string|null|undefined {
    return getAnnouncementBlock(blockId).querySelector('p')?.textContent;
  }

  function getAnnouncementBlockNumParagraphs(blockId: string): number {
    return getAnnouncementBlock(blockId).querySelectorAll('p').length;
  }

  function getButton<T extends HTMLElement>(id: string): T {
    return toolbar.$.fontSizeMenu.get().querySelector<T>(`#${id}`)!;
  }

  // Clicks the button and, like the app would, passes the new size back down.
  async function clickIncrease(): Promise<void> {
    getButton<CrIconButtonElement>('font-size-increase').click();
    toolbar.fontSize++;
    await microtasksFinished();
  }

  async function clickDecrease(): Promise<void> {
    getButton<CrIconButtonElement>('font-size-decrease').click();
    toolbar.fontSize--;
    await microtasksFinished();
  }

  test('increase clicked fires event', async () => {
    const whenFired = eventToPromise<CustomEvent<{data: boolean}>>(
        ToolbarEvent.FONT_SIZE, toolbar);

    await clickIncrease();

    const event = await whenFired;
    assertTrue(event.detail.data);
  });

  test('increase clicked changes aria-live region', async () => {
    await clickIncrease();
    assertEquals(
        'Font size increased', getAnnouncementBlockText('#size-announce'));
  });

  test('decrease clicked fires event', async () => {
    const whenFired = eventToPromise<CustomEvent<{data: boolean}>>(
        ToolbarEvent.FONT_SIZE, toolbar);

    await clickDecrease();

    const event = await whenFired;
    assertFalse(event.detail.data);
  });

  test('aria-live region is not visible', async () => {
    let announceBlock = getAnnouncementBlock('#size-announce');
    let width = announceBlock.offsetWidth;
    let height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);

    await clickDecrease();

    announceBlock = getAnnouncementBlock('#size-announce');
    assertEquals(1, announceBlock.querySelectorAll('p').length);
    width = announceBlock.offsetWidth;
    height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);
  });

  test('font size clicked keeps adding to aria-live region', async () => {
    await clickIncrease();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    assertEquals(2, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    assertEquals(3, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    assertEquals(4, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    assertEquals(5, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    // After 6 calls to increase size, the div will be cleared.
    assertEquals(0, getAnnouncementBlockNumParagraphs('#size-announce'));

    await clickDecrease();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));
  });

  test('decrease clicked changes aria-live region', async () => {
    await clickDecrease();
    assertEquals(
        'Font size decreased', getAnnouncementBlockText('#size-announce'));
  });

  test('click that does not change the size is not announced', async () => {
    getButton<CrIconButtonElement>('font-size-increase').click();
    await microtasksFinished();

    assertEquals(0, getAnnouncementBlockNumParagraphs('#size-announce'));
  });

  test('reset clicked fires event', async () => {
    await clickIncrease();

    const whenFired = eventToPromise(ToolbarEvent.FONT_SIZE_RESET, toolbar);
    getButton<CrButtonElement>('font-size-reset').click();
    await whenFired;
  });

  test(
      'reset button disabled state updates based on default font size',
      async () => {
        // Open the font size settings drop-down menu
        stubAnimationFrame();
        menuButton!.click();
        await microtasksFinished();

        const resetButton = getButton<CrButtonElement>('font-size-reset');
        assertTrue(!!resetButton, 'Reset button should be present in DOM');
        assertTrue(
            resetButton.disabled,
            'Reset button should be disabled at default font size');

        await clickIncrease();
        assertFalse(
            resetButton.disabled,
            'Reset button should be enabled when font size changes');

        toolbar.fontSize = visualBrowserProxy.defaultFontSize;
        await microtasksFinished();
        assertTrue(
            resetButton.disabled,
            'Reset button should become disabled after resetting font size');
      });

  test('decrease button disabled at min font size', async () => {
    stubAnimationFrame();
    menuButton!.click();
    await microtasksFinished();

    const decreaseButton = getButton<CrIconButtonElement>('font-size-decrease');
    assertFalse(decreaseButton.disabled);

    toolbar.fontSize = visualBrowserProxy.minFontSize + 1;
    await clickDecrease();
    assertTrue(decreaseButton.disabled);

    await clickIncrease();
    assertFalse(decreaseButton.disabled);
  });

  test('increase button disabled at max font size', async () => {
    stubAnimationFrame();
    menuButton!.click();
    await microtasksFinished();

    const increaseButton = getButton<CrIconButtonElement>('font-size-increase');
    assertFalse(increaseButton.disabled);

    toolbar.fontSize = visualBrowserProxy.maxFontSize - 1;
    await clickIncrease();
    assertTrue(increaseButton.disabled);

    await clickDecrease();
    assertFalse(increaseButton.disabled);
  });
});
