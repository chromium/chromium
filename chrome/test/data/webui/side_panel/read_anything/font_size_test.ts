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

import {setupTestEnvironment} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('FontSize', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement|null;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(async () => {
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;

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

  function clickIncrease(): void {
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-increase')!.click();
  }

  function clickDecrease(): void {
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrIconButtonElement>('#font-size-decrease')!.click();
  }

  test('increase clicked fires events', async () => {
    const whenFired = eventToPromise(ToolbarEvent.FONT_SIZE, toolbar);

    clickIncrease();

    await whenFired;
    assertTrue(await visualBrowserProxy.whenCalled('onFontSizeChanged'));
  });

  test('increase clicked changes aria-live region', () => {
    clickIncrease();
    assertEquals(
        'Font size increased', getAnnouncementBlockText('#size-announce'));
  });

  test('decrease clicked first events', async () => {
    const whenFired = eventToPromise(ToolbarEvent.FONT_SIZE, toolbar);

    clickDecrease();

    await whenFired;
    assertFalse(await visualBrowserProxy.whenCalled('onFontSizeChanged'));
  });

  test('aria-live region is not visible', () => {
    let announceBlock = getAnnouncementBlock('#size-announce');
    let width = announceBlock.offsetWidth;
    let height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);

    clickDecrease();

    announceBlock = getAnnouncementBlock('#size-announce');
    assertEquals(1, announceBlock.querySelectorAll('p').length);
    width = announceBlock.offsetWidth;
    height = announceBlock.offsetHeight;
    assertGT(2, width);
    assertGT(2, height);
  });

  test('font size clicked keeps adding to aria-live region', () => {
    clickIncrease();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    assertEquals(2, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    assertEquals(3, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    assertEquals(4, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    assertEquals(5, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    // After 6 calls to increase size, the div will be cleared.
    assertEquals(0, getAnnouncementBlockNumParagraphs('#size-announce'));

    clickDecrease();
    assertEquals(1, getAnnouncementBlockNumParagraphs('#size-announce'));
  });

  test(' decrease clicked changes aria-live region', () => {
    clickDecrease();
    assertEquals(
        toolbar.$.toolbarContainer.querySelector('#size-announce')
            ?.querySelector('p')
            ?.textContent,
        'Font size decreased');
  });

  test('reset clicked returns font size to starting size', async () => {
    clickIncrease();
    assertTrue(await visualBrowserProxy.whenCalled('onFontSizeChanged'));
    visualBrowserProxy.reset();
    clickIncrease();
    assertTrue(await visualBrowserProxy.whenCalled('onFontSizeChanged'));

    const whenFired = eventToPromise(ToolbarEvent.FONT_SIZE, toolbar);
    toolbar.$.fontSizeMenu.get()
        .querySelector<CrButtonElement>('#font-size-reset')!.click();
    await whenFired;

    assertEquals(1, visualBrowserProxy.getCallCount('onFontSizeReset'));
  });

  test(
      'reset button disabled state updates based on default font size',
      async () => {
        // Force a re-render of the toolbar element
        toolbar.requestUpdate();
        await microtasksFinished();

        // Open the font size settings drop-down menu
        menuButton!.click();
        await microtasksFinished();

        const resetButton =
            toolbar.$.fontSizeMenu.get().querySelector<CrButtonElement>(
                '#font-size-reset')!;
        assertTrue(!!resetButton, 'Reset button should be present in DOM');

        // Assertion: Reset button must be disabled initially because font
        // size is at its default value
        assertTrue(
            resetButton.disabled,
            'Reset button should be disabled at default font size');

        clickIncrease();
        await visualBrowserProxy.whenCalled('onFontSizeChanged');
        visualBrowserProxy.fontSize++;
        await microtasksFinished();
        assertFalse(
            resetButton.disabled,
            'Reset button should be enabled when font size changes');

        // Interaction: Click the reset button to revert to default font size
        resetButton.click();
        await visualBrowserProxy.whenCalled('onFontSizeReset');
        visualBrowserProxy.fontSize = visualBrowserProxy.defaultFontSize;
        await microtasksFinished();
        assertTrue(
            resetButton.disabled,
            'Reset button should become disabled after resetting font size');
      });
});
