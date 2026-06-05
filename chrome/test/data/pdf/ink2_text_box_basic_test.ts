// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, MIN_TEXTBOX_SIZE_PX, TEXT_COLORS, TextAlignment, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, dragHandle, initializeBox, setupTextBoxTest} from './ink2_text_box_test_utils.js';
import {getRequiredElement} from './test_util.js';
chrome.test.runTests([
  // Test drawing the box based on data from the manager.
  async function testDrawsBox() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initial state. Textbox is not visible because it hasn't received an
    // initialize-text-box event yet.
    chrome.test.assertTrue(textbox.hidden);
    // Check both the textbox and inner <textarea> to confirm styling is set
    // correctly. The <textarea> should have the size specified as its width
    // and height style. The ink-text-box is positioned to account for padding
    // and border offsets, and is larger to fit the padding inside.
    // x: 10px UX padding + 2px border + 5px outer padding = 17px offset
    // y: 8px UX padding + 2px border + 5px outer padding = 15px offset
    // Outer padding is excluded in width and height computed style. So the
    // width = 2 * x - 2 * outer padding = 24px + specified width
    // height = 2 * y - 2 * outer padding = 20px + specified height

    // Create a 160x40 box at 80, 120.
    initializeBox(manager, 160, 40, 80, 120);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    assertPositionAndSize(textbox, '184px', '60px', '63px', '105px');
    chrome.test.assertEq(194, textbox.clientWidth);
    chrome.test.assertEq(70, textbox.clientHeight);
    assertPositionAndSize(textbox.$.textbox, '160px', '40px', 'auto', 'auto');
    // Inner textarea has 5px inline padding and 3px vertical padding to match
    // UX specified 10px and 8px total.
    chrome.test.assertEq(170, textbox.$.textbox.clientWidth);
    chrome.test.assertEq(46, textbox.$.textbox.clientHeight);
    chrome.test.assertEq('', textbox.$.textbox.value);

    // Update to a 100x200 box at 400, 300 with "Hello World" text.
    initializeBox(manager, 100, 200, 400, 300);
    await microtasksFinished();
    textbox.$.textbox.value = 'Hello World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');
    chrome.test.assertEq(134, textbox.clientWidth);
    chrome.test.assertEq(230, textbox.clientHeight);
    assertPositionAndSize(textbox.$.textbox, '100px', '200px', 'auto', 'auto');
    chrome.test.assertEq(110, textbox.$.textbox.clientWidth);
    chrome.test.assertEq(206, textbox.$.textbox.clientHeight);
    chrome.test.assertEq('Hello World', textbox.$.textbox.value);
    chrome.test.succeed();
  },

  // Test that the textbox styles change based on an update event.
  async function testTextbox() {
    const {manager, textbox} = await setupTextBoxTest();
    // Update to a 100x200 box at 400, 300.
    initializeBox(manager, 100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertEq('', textbox.$.textbox.value);
    const textboxStyles = getComputedStyle(textbox.$.textbox);

    // Initial state
    chrome.test.assertEq('12px', textboxStyles.getPropertyValue('font-size'));
    chrome.test.assertEq(
        'Arial, sans-serif', textboxStyles.getPropertyValue('font-family'));
    chrome.test.assertEq('400', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'normal', textboxStyles.getPropertyValue('font-style'));
    chrome.test.assertEq('left', textboxStyles.getPropertyValue('text-align'));
    const textDecorationLine =
        textboxStyles.getPropertyValue('text-decoration-line');
    chrome.test.assertTrue(
        textDecorationLine === 'none' || textDecorationLine === 'initial');
    const color = hexToColor(TEXT_COLORS[0]!.color);
    const colorStyle = `rgb(${color.r}, ${color.g}, ${color.b})`;
    chrome.test.assertEq(colorStyle, textboxStyles.getPropertyValue('color'));

    // Confirm updating styles in the manager updates the style of the textbox.
    // Each type of update should independently trigger a change.
    // Typeface
    manager.setTextTypeface(TextTypeface.SERIF);
    await microtasksFinished();
    chrome.test.assertEq(
        'Times, serif', textboxStyles.getPropertyValue('font-family'));

    // Size
    manager.setTextSize(20);
    await microtasksFinished();
    chrome.test.assertEq('20px', textboxStyles.getPropertyValue('font-size'));

    // Styles
    manager.setTextStyles({
      [TextStyle.BOLD]: true,
      [TextStyle.ITALIC]: true,
    });
    await microtasksFinished();
    chrome.test.assertEq('700', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'italic', textboxStyles.getPropertyValue('font-style'));

    // Color
    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    manager.setTextColor(newColor);
    const newColorStyle = `rgb(${newColor.r}, ${newColor.g}, ${newColor.b})`;
    chrome.test.assertEq(
        newColorStyle, textboxStyles.getPropertyValue('color'));

    // Alignment
    manager.setTextAlignment(TextAlignment.RIGHT);
    await microtasksFinished();
    chrome.test.assertEq('right', textboxStyles.getPropertyValue('text-align'));
    chrome.test.succeed();
  },

  async function testDragHandles() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x200 box at 400, 300.
    initializeBox(manager, 100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');

    // Drag the top left handle up and left to make the box 100px bigger in
    // each dimension.
    const topLeft = getRequiredElement(textbox, '.handle.top.left');
    await dragHandle(topLeft, -100, -100);
    assertPositionAndSize(textbox, '224px', '320px', '283px', '185px');

    // Try to drag the top left handle down and right to make the box too small.
    // It should clamp at the size needed to render the text box in the bottom
    // right corner (at 500, 500);
    await dragHandle(topLeft, 400, 400);
    // Note these include padding.
    const clampedTextareaWidth = textbox.$.textbox.clientWidth;
    const clampedTextareaHeight = textbox.$.textbox.clientHeight;
    // Inner border padding + border = 14px
    const clampedHeight = clampedTextareaHeight + 14;
    const clampedWidth = clampedTextareaWidth + 14;
    chrome.test.assertTrue(
        clampedTextareaHeight >= textbox.$.textbox.scrollHeight);
    chrome.test.assertEq(MIN_TEXTBOX_SIZE_PX + 10, clampedTextareaWidth);
    // yCorner - textareaInnerHeight - yOffset
    const clampedTop = 500 - (clampedTextareaHeight - 6) - 15;
    // clampedLeft = 500 - MIN_TEXTBOX_SIZE_PX - 17
    assertPositionAndSize(
        textbox, `${clampedWidth}px`, `${clampedHeight}px`, '459px',
        `${clampedTop}px`);

    // Drag the top handle up and left to make the box 212px tall. Left
    // motion is ignored.
    const top = getRequiredElement(textbox, '.handle.top.center');
    await dragHandle(top, -100, -212 + (clampedTextareaHeight - 6));
    // height 212, width the same, x same, y 288
    assertPositionAndSize(
        textbox, `${clampedWidth}px`, '232px', '459px', '273px');

    // Drag the top right handle down and right to make the box 12px shorter
    // and 100px wide.
    const topRight = getRequiredElement(textbox, '.handle.top.right');
    await dragHandle(topRight, 76, 12);
    assertPositionAndSize(textbox, '124px', '220px', '459px', '285px');

    // Drag the left handle right and up. Upward motion is ignored. Left motion
    // makes the box 40px narrower.
    const left = getRequiredElement(textbox, '.handle.left.center');
    await dragHandle(left, 40, -200);
    assertPositionAndSize(textbox, '84px', '220px', '499px', '285px');

    // Drag the right handle right and down. Downward motion is ignored. Right
    // motion makes the box 100px wider.
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandle(right, 100, 100);
    assertPositionAndSize(textbox, '184px', '220px', '499px', '285px');

    // Drag the bottom left handle down and left to make the box 100px bigger
    // in both dimensions.
    const bottomLeft = getRequiredElement(textbox, '.handle.bottom.left');
    await dragHandle(bottomLeft, -100, 100);
    assertPositionAndSize(textbox, '284px', '320px', '399px', '285px');

    // Drag the bottom handle down and left to make the box 100px taller.
    // Motion left is ignored.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandle(bottom, -100, 100);
    assertPositionAndSize(textbox, '284px', '420px', '399px', '285px');

    // Drag the bottom right handle down and right to make the box 20px bigger
    // in both dimensions.
    const bottomRight = getRequiredElement(textbox, '.handle.bottom.right');
    await dragHandle(bottomRight, 20, 20);
    assertPositionAndSize(textbox, '304px', '440px', '399px', '285px');

    // Drag the bottom right handle up and left to try to make the box too
    // small. Make sure it clamps at the same minimum size, anchored on the top
    // left corner.
    await dragHandle(bottomRight, -400, -400);
    assertPositionAndSize(
        textbox, `${clampedWidth}px`, `${clampedHeight}px`, '399px', '285px');

    chrome.test.succeed();
  },

  async function testAutoResize() {
    const {manager, textbox} = await setupTextBoxTest();
    initializeBox(manager, 24, 24, 416, 300);
    await microtasksFinished();
    // Textbox is initialized to the minimum clamped size.
    const clampedTextareaWidth = textbox.$.textbox.clientWidth;
    const clampedTextareaHeight = textbox.$.textbox.clientHeight;
    // Add 10 to min size for measured clientWidth due to padding.
    chrome.test.assertEq(MIN_TEXTBOX_SIZE_PX + 10, clampedTextareaWidth);
    assertPositionAndSize(
        textbox, `${clampedTextareaWidth + 14}px`,
        `${clampedTextareaHeight + 14}px`, '399px', '285px');

    // Simulate putting in a really long input that won't fit in the clamped
    // size.
    textbox.$.textbox.value = 'A much longer string than the original sample ' +
        'text, which does not fit in the current small textbox size.';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    const updatedTextareaWidth = textbox.$.textbox.clientWidth;
    const updatedTextareaHeight = textbox.$.textbox.clientHeight;
    const updatedHeight = updatedTextareaHeight + 14;
    chrome.test.assertTrue(updatedTextareaHeight > clampedTextareaHeight);
    chrome.test.assertTrue(
        updatedTextareaHeight >= textbox.$.textbox.scrollHeight);
    chrome.test.assertEq(MIN_TEXTBOX_SIZE_PX + 10, updatedTextareaWidth);

    // Make sure that if the user makes the box wider, they can then make it
    // shorter.
    const right = getRequiredElement(textbox, '.handle.right.center');
    // Makes the textarea 300px wide.
    await dragHandle(right, 276, 0);
    // Wider box is still just as tall since the user didn't resize it
    // vertically yet.
    assertPositionAndSize(
        textbox, '324px', `${updatedHeight}px`, '399px', '285px');

    // User should now be able to shrink the box vertically, since the text
    // should fit in a shorter height with the updated width.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandle(bottom, 0, -100);
    assertPositionAndSize(
        textbox, '324px', `${updatedHeight - 100}px`, '399px', '285px');

    chrome.test.succeed();
  },

  async function testResizeClampedToPageBoundaries() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');

    // Test left edge. First resize with the left handle and ensure that the
    // box size clamps at the start of the page. Then try dragging the top left
    // and bottom left handles further left and ensure they don't resize the
    // box to the left either.
    const left = getRequiredElement(textbox, '.handle.left.center');
    await dragHandle(left, -500, 0);
    // pageX = 10, so edge of the text is at 10 and the box is at -7.
    // This means we increased the width by 400 - 10 = 390.
    assertPositionAndSize(textbox, '514px', '120px', '-7px', '285px');
    const topLeft = getRequiredElement(textbox, '.handle.left.top');
    await dragHandle(topLeft, -50, -50);
    assertPositionAndSize(textbox, '514px', '170px', '-7px', '235px');
    const bottomLeft = getRequiredElement(textbox, '.handle.left.bottom');
    await dragHandle(bottomLeft, -50, -50);
    assertPositionAndSize(textbox, '514px', '120px', '-7px', '235px');

    // Test top edge. First resize with the top left handle and ensure that the
    // box size clamps at the top of the page. Then try dragging the top center
    // and top right handles further up, and ensure they don't resize the box
    // upward.
    await dragHandle(topLeft, -50, -300);
    // pageY = 3, so edge of the text is at 3 and the box is at -12.
    // This means we increased the height by 250 - 3 = 247. Note that the left
    // motion is also ignored, as we are at the page corner.
    assertPositionAndSize(textbox, '514px', '367px', '-7px', '-12px');
    const top = getRequiredElement(textbox, '.handle.top.center');
    await dragHandle(top, -50, -50);
    assertPositionAndSize(textbox, '514px', '367px', '-7px', '-12px');
    const topRight = getRequiredElement(textbox, '.handle.top.right');
    await dragHandle(topRight, -50, -50);
    assertPositionAndSize(textbox, '464px', '367px', '-7px', '-12px');

    // Use the top left handle to ensure the box can be moved off of the top
    // left corner.
    await dragHandle(topLeft, 200, 200);
    assertPositionAndSize(textbox, '264px', '167px', '193px', '188px');

    // Test the right edge. First resize with the bottom right handle and ensure
    // that the box clamps at the right edge of the page. Then try dragging the
    // right center and top right handles, and ensure they don't resize the box
    // to the right.
    const bottomRight = getRequiredElement(textbox, '.handle.bottom.right');
    await dragHandle(bottomRight, 600, 100);
    // Right page edge is at 1000 + 10 = 1010 and the left edge of the text is
    // at 193 + 17 = 210. This means the maximum width is 800px for the text, or
    // 824px for the box.
    assertPositionAndSize(textbox, '824px', '267px', '193px', '188px');
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandle(right, 100, 0);
    assertPositionAndSize(textbox, '824px', '267px', '193px', '188px');
    await dragHandle(topRight, 100, 100);
    assertPositionAndSize(textbox, '824px', '167px', '193px', '288px');

    // Use the left handle to narrow the box.
    await dragHandle(left, 500, 0);
    assertPositionAndSize(textbox, '324px', '167px', '693px', '288px');

    // Test the bottom edge. Use the bottom handle and ensure the box clamps
    // at the bottom of the page, then ensure the bottom left and bottom right
    // handles can't move it down from there.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandle(bottom, 20, 800);
    // Bottom edge of the page is at 1000 + 3 = 1003 and the top edge of the
    // text is at 288 + 15 = 303. This means the maximum height is 700px for
    // the text, or 720px for the box.
    assertPositionAndSize(textbox, '324px', '720px', '693px', '288px');
    // Both directions are ignored for the bottom right handle since the box
    // is in the bottom right corner.
    await dragHandle(bottomRight, 100, 100);
    assertPositionAndSize(textbox, '324px', '720px', '693px', '288px');
    const bottomLeft2 = getRequiredElement(textbox, '.handle.bottom.left');
    await dragHandle(bottomLeft2, 100, 100);
    assertPositionAndSize(textbox, '224px', '720px', '793px', '288px');

    chrome.test.succeed();
  },

  async function testMove() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');
    await dragHandle(textbox, 100, 100);
    assertPositionAndSize(textbox, '124px', '120px', '483px', '385px');
    await dragHandle(textbox, -200, 100);
    assertPositionAndSize(textbox, '124px', '120px', '283px', '485px');
    await dragHandle(textbox, 0, -200);
    assertPositionAndSize(textbox, '124px', '120px', '283px', '285px');

    // Make sure that clicking and trying to drag the textarea itself does
    // not move the textbox.
    await dragHandle(textbox.$.textbox, -200, -200);
    assertPositionAndSize(textbox, '124px', '120px', '283px', '285px');
    chrome.test.succeed();
  },

  async function testMoveToPageBoundaries() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');

    // Drag the box past the top left corner and ensure it stops moving.
    // Default pageX and pageY are 10 and 3. The top left corner of the text
    // should be there, which puts the box at -7, -12
    // x: 10px UX padding + 2px border + 5px outer padding = 17px offset
    // y: 8px UX padding + 2px border + 5px outer padding = 15px offset
    await dragHandle(textbox, -500, -500);
    assertPositionAndSize(textbox, '124px', '120px', '-7px', '-12px');

    // Drag the box past the top right corner and ensure it stops moving. The
    // right page edge is at pageX + pageWidth = 1010, so the left edge of
    // the text is at 1010 - 100 = 910, putting the box edge at 893.
    await dragHandle(textbox, 1100, -100);
    assertPositionAndSize(textbox, '124px', '120px', '893px', '-12px');

    // Drag the box past the bottom left corner and ensure it stops moving. The
    // bottom page edge is at pageY + pageHeight = 1003, so the top edge of
    // the text is at 1003 - 100 = 903, putting the box edge at 888.
    await dragHandle(textbox, -1200, 1200);
    assertPositionAndSize(textbox, '124px', '120px', '-7px', '888px');

    // Drag the box past the bottom right corner and ensure it stops moving.
    await dragHandle(textbox, 1200, 200);
    assertPositionAndSize(textbox, '124px', '120px', '893px', '888px');

    chrome.test.succeed();
  },
]);
