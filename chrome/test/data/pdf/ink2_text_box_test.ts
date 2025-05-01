// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, TEXT_COLORS, TextAlignment, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getRequiredElement, setupTestViewportAndMockPluginForInk} from './test_util.js';

// Set up a dummy viewport so that we can get a predictable initial state.
setupTestViewportAndMockPluginForInk();
const manager = Ink2Manager.getInstance();
// Initialize a font, since this starts out empty.
manager.setTextTypeface('Roboto');
const textbox = document.createElement('ink-text-box');
document.body.appendChild(textbox);

function initializeBox(
    width: number, height: number, x: number, y: number, existing?: boolean) {
  manager.dispatchEvent(new CustomEvent('initialize-text-box', {
    detail: {
      annotation: {
        text: existing ? 'Hello World' : '',
        textAttributes: {
          size: 12,
          typeface: 'Roboto',
          styles: {
            [TextStyle.BOLD]: false,
            [TextStyle.ITALIC]: false,
            [TextStyle.UNDERLINE]: false,
            [TextStyle.STRIKETHROUGH]: false,
          },
          alignment: TextAlignment.LEFT,
          color: hexToColor(TEXT_COLORS[0]!.color),
        },
        textBoxRect: {height, locationX: x, locationY: y, width},
        id: 0,
        pageNumber: 0,
      },
      pageCoordinates: {x: 10, y: 3},
    },
  }));
}

function assertPositionAndSize(
    el: HTMLElement, expectedWidth: string, expectedHeight: string,
    expectedLeft: string, expectedTop: string) {
  const styles = getComputedStyle(el);
  chrome.test.assertEq(expectedWidth, styles.getPropertyValue('width'));
  chrome.test.assertEq(expectedHeight, styles.getPropertyValue('height'));
  chrome.test.assertEq(expectedLeft, styles.getPropertyValue('left'));
  chrome.test.assertEq(expectedTop, styles.getPropertyValue('top'));
}

async function dragHandle(handle: HTMLElement, deltaX: number, deltaY: number) {
  // Simulate events in the same order they are fired by the browser.
  // Need to provide a valid |pointerId| for setPointerCapture() to not
  // throw an error. Using arbitrary start position for the pointer, since
  // only the change matters.
  handle.dispatchEvent(new PointerEvent(
      'pointerdown', {composed: true, pointerId: 1, clientX: 50, clientY: 40}));
  // Send a few move events to better simulate reality. Allow the code
  // updating the minimum height time to run in between.
  for (let i = 1; i <= 4; i++) {
    handle.dispatchEvent(new PointerEvent('pointermove', {
      pointerId: 1,
      clientX: 50 + deltaX * i / 4,
      clientY: 40 + deltaY * i / 4,
    }));
    await microtasksFinished();
  }
  handle.dispatchEvent(new PointerEvent(
      'pointerup', {pointerId: 1, clientX: 50 + deltaX, clientY: 40 + deltaY}));
  await microtasksFinished();
}

chrome.test.runTests([
  // Test drawing the box based on data from the manager.
  async function testDrawsBox() {
    // Initial state. Textbox is not visible because it hasn't received an
    // update-text-box event yet.
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    // Create a 160x40 box at 80, 120.
    initializeBox(160, 40, 80, 120);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '160px', '40px', '80px', '120px');

    // Update to a 100x200 box at 400, 300 with existing "Hello World" text.
    initializeBox(100, 200, 400, 300, true);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '100px', '200px', '400px', '300px');
    chrome.test.assertEq('Hello World', textbox.$.textbox.value);
    chrome.test.succeed();
  },

  // Test that the textbox styles change based on an update event.
  async function testTextbox() {
    // Update to a 100x200 box at 400, 300.
    initializeBox(100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Sample Text', textbox.$.textbox.value);
    const textboxStyles = getComputedStyle(textbox.$.textbox);

    // Initial state
    chrome.test.assertEq('12px', textboxStyles.getPropertyValue('font-size'));
    chrome.test.assertEq(
        'Roboto', textboxStyles.getPropertyValue('font-family'));
    chrome.test.assertEq('400', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'normal', textboxStyles.getPropertyValue('font-style'));
    chrome.test.assertEq('left', textboxStyles.getPropertyValue('text-align'));
    chrome.test.assertTrue(
        textboxStyles.getPropertyValue('text-decoration').includes('none'));
    const color = hexToColor(TEXT_COLORS[0]!.color);
    const colorStyle = `rgb(${color.r}, ${color.g}, ${color.b})`;
    chrome.test.assertEq(colorStyle, textboxStyles.getPropertyValue('color'));

    // Confirm updating styles in the manager updates the style of the textbox.
    manager.setTextTypeface('Serif');
    manager.setTextSize(20);
    manager.setTextStyles({
      [TextStyle.BOLD]: true,
      [TextStyle.ITALIC]: true,
      [TextStyle.UNDERLINE]: true,
      [TextStyle.STRIKETHROUGH]: false,
    });
    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    manager.setTextColor(newColor);
    manager.setTextAlignment(TextAlignment.RIGHT);
    await microtasksFinished();
    chrome.test.assertEq('20px', textboxStyles.getPropertyValue('font-size'));
    chrome.test.assertEq(
        'serif', textboxStyles.getPropertyValue('font-family'));
    chrome.test.assertEq('700', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'italic', textboxStyles.getPropertyValue('font-style'));
    chrome.test.assertEq('right', textboxStyles.getPropertyValue('text-align'));
    chrome.test.assertTrue(textboxStyles.getPropertyValue('text-decoration')
                               .includes('underline'));
    const newColorStyle = `rgb(${newColor.r}, ${newColor.g}, ${newColor.b})`;
    chrome.test.assertEq(
        newColorStyle, textboxStyles.getPropertyValue('color'));

    chrome.test.succeed();
  },

  async function testDragHandles() {
    // Initialize to a 100x200 box at 400, 300.
    initializeBox(100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '100px', '200px', '400px', '300px');

    // Drag the top left handle up and left to make the box 100px bigger in
    // each dimension.
    const topLeft = getRequiredElement(textbox, '.handle.top.left');
    await dragHandle(topLeft, -100, -100);
    assertPositionAndSize(textbox, '200px', '300px', '300px', '200px');

    // Try to drag the top left handle down and right to make the box too small.
    // It should clamp at the size needed to render the text box in the bottom
    // right corner (at 500, 500);
    await dragHandle(topLeft, 400, 400);
    const clampedWidth = textbox.$.textbox.clientWidth;
    const clampedHeight = textbox.$.textbox.clientHeight;
    chrome.test.assertTrue(clampedHeight >= textbox.$.textbox.scrollHeight);
    // Min width is 36px.
    chrome.test.assertEq(36, clampedWidth);
    const clampedTop = 500 - clampedHeight;
    assertPositionAndSize(
        textbox, '36px', `${clampedHeight}px`, '464px', `${clampedTop}px`);

    // Drag the top handle up and left to make the box 212px tall. Left
    // motion is ignored.
    const top = getRequiredElement(textbox, '.handle.top.center');
    await dragHandle(top, -100, -212 + clampedHeight);
    // height 212, width the same, x same, y 288
    assertPositionAndSize(textbox, '36px', '212px', '464px', '288px');

    // Drag the top right handle down and right to make the box 12px shorter
    // and 100px wide.
    const topRight = getRequiredElement(textbox, '.handle.top.right');
    await dragHandle(topRight, 64, 12);
    assertPositionAndSize(textbox, '100px', '200px', '464px', '300px');

    // Drag the left handle right and up. Upward motion is ignored. Left motion
    // makes the box 40px narrower.
    const left = getRequiredElement(textbox, '.handle.left.center');
    await dragHandle(left, 40, -200);
    assertPositionAndSize(textbox, '60px', '200px', '504px', '300px');

    // Drag the right handle right and down. Downward motion is ignored. Right
    // motion makes the box 100px wider.
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandle(right, 100, 100);
    assertPositionAndSize(textbox, '160px', '200px', '504px', '300px');

    // Drag the bottom left handle down and left to make the box 100px bigger
    // in both dimensions.
    const bottomLeft = getRequiredElement(textbox, '.handle.bottom.left');
    await dragHandle(bottomLeft, -100, 100);
    assertPositionAndSize(textbox, '260px', '300px', '404px', '300px');

    // Drag the bottom handle down and left to make the box 100px taller.
    // Motion left is ignored.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandle(bottom, -100, 100);
    assertPositionAndSize(textbox, '260px', '400px', '404px', '300px');

    // Drag the bottom right handle down and right to make the box 20px bigger
    // in both dimensions.
    const bottomRight = getRequiredElement(textbox, '.handle.bottom.right');
    await dragHandle(bottomRight, 20, 20);
    assertPositionAndSize(textbox, '280px', '420px', '404px', '300px');

    // Drag the bottom right handle up and left to try to make the box too
    // small. Make sure it clamps at the same minimum size, anchored on the top
    // left corner.
    await dragHandle(bottomRight, -400, -400);
    assertPositionAndSize(
        textbox, '36px', `${clampedHeight}px`, '404px', '300px');

    chrome.test.succeed();
  },

  async function testAutoResize() {
    // Textbox is in clamped size from the previous test.
    const clampedWidth = textbox.$.textbox.clientWidth;
    const clampedHeight = textbox.$.textbox.clientHeight;
    chrome.test.assertEq(36, clampedWidth);
    assertPositionAndSize(
        textbox, '36px', `${clampedHeight}px`, '404px', '300px');

    // Simulate putting in a really long input that won't fit in the clamped
    // size.
    textbox.$.textbox.value = 'A much longer string than the original sample ' +
        'text, which does not fit in the current small textbox size.';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    const updatedWidth = textbox.$.textbox.clientWidth;
    const updatedHeight = textbox.$.textbox.clientHeight;
    chrome.test.assertTrue(updatedHeight > clampedHeight);
    chrome.test.assertTrue(updatedHeight >= textbox.$.textbox.scrollHeight);
    chrome.test.assertEq(36, updatedWidth);

    // Make sure that if the user makes the box wider, they can then make it
    // shorter.
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandle(right, 264, 0);
    // Wider box is still just as tall since the user didn't resize it
    // vertically yet.
    assertPositionAndSize(
        textbox, '300px', `${updatedHeight}px`, '404px', '300px');

    // User should now be able to shrink the box vertically, since the text
    // should fit in a shorter height with the updated width.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandle(bottom, 0, -100);
    assertPositionAndSize(
        textbox, '300px', `${updatedHeight - 100}px`, '404px', '300px');

    // Reset the sample text for later tests.
    textbox.$.textbox.value = 'Sample Text';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    chrome.test.succeed();
  },

  async function testMove() {
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '400px', '300px');
    await dragHandle(textbox, 100, 100);
    assertPositionAndSize(textbox, '100px', '100px', '500px', '400px');
    await dragHandle(textbox, -200, 100);
    assertPositionAndSize(textbox, '100px', '100px', '300px', '500px');
    await dragHandle(textbox, 0, -200);
    assertPositionAndSize(textbox, '100px', '100px', '300px', '300px');

    // Make sure that clicking and trying to drag the textarea itself does
    // not move the textbox.
    await dragHandle(textbox.$.textbox, -200, -200);
    assertPositionAndSize(textbox, '100px', '100px', '300px', '300px');
    chrome.test.succeed();
  },

  async function testViewportChanges() {
    // Initialize to a 100x100 box at 410, 303.
    initializeBox(100, 100, 410, 303);
    await microtasksFinished();

    assertPositionAndSize(textbox, '100px', '100px', '410px', '303px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 0.5. This also comes with x and y changes
    // simulating production.
    manager.dispatchEvent(new CustomEvent(
        'viewport-changed', {detail: {pageX: 30, pageY: 1.5, zoom: 0.5}}));
    await microtasksFinished();
    assertPositionAndSize(textbox, '50px', '50px', '230px', '151.5px');
    chrome.test.assertEq(
        '6px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 2.0. This also comes with x and y changes
    // simulating production.
    manager.dispatchEvent(new CustomEvent(
        'viewport-changed', {detail: {pageX: 10, pageY: 6, zoom: 2.0}}));
    await microtasksFinished();
    assertPositionAndSize(textbox, '200px', '200px', '810px', '606px');
    chrome.test.assertEq(
        '24px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a scroll + resetting zoom to 1.0.
    manager.dispatchEvent(new CustomEvent(
        'viewport-changed', {detail: {pageX: 100, pageY: 100, zoom: 1.0}}));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '500px', '400px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where start of page is no longer in the viewport.
    manager.dispatchEvent(new CustomEvent(
        'viewport-changed', {detail: {pageX: -100, pageY: -100, zoom: 1.0}}));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '300px', '200px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where textbox ends up off screen.
    manager.dispatchEvent(new CustomEvent(
        'viewport-changed', {detail: {pageX: -500, pageY: -500, zoom: 1.0}}));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '-100px', '-200px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));
    chrome.test.succeed();
  },
]);
