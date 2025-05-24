// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, PluginController, PluginControllerEventType, TEXT_COLORS, TextAlignment, TextBoxState, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {TextAnnotation} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertDeepEquals, getRequiredElement, setUpInkTestContext} from './test_util.js';

// Set up a dummy viewport so that we can get a predictable initial state.
const {viewport, mockPlugin} = setUpInkTestContext();
const manager = Ink2Manager.getInstance();
manager.initializeTextAnnotations();
const textbox = document.createElement('ink-text-box');
document.body.appendChild(textbox);

function getDefaultAnnotation(): TextAnnotation {
  return {
    text: 'Hello World',
    textAttributes: {
      size: 12,
      typeface: TextTypeface.SANS_SERIF,
      styles: {
        [TextStyle.BOLD]: false,
        [TextStyle.ITALIC]: false,
      },
      alignment: TextAlignment.LEFT,
      color: hexToColor(TEXT_COLORS[0]!.color),
    },
    textBoxRect: {height: 100, locationX: 400, locationY: 300, width: 100},
    textOrientation: 0,
    id: 0,
    pageNumber: 0,
  };
}

function initializeBox(
    width: number, height: number, x: number, y: number, existing?: boolean,
    orientation?: number) {
  const annotation = getDefaultAnnotation();
  if (!existing) {
    annotation.text = '';
  }
  if (orientation) {
    annotation.textOrientation = orientation;
  }
  annotation.textBoxRect.locationX = x;
  annotation.textBoxRect.locationY = y;
  annotation.textBoxRect.width = width;
  annotation.textBoxRect.height = height;

  manager.dispatchEvent(new CustomEvent('initialize-text-box', {
    detail: {
      annotation,
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

async function dragHandleWithKeyboard(
    handle: HTMLElement, key: string, numEvents: number,
    useFocusOut: boolean = false) {
  for (let i = 0; i < numEvents; i++) {
    keyDownOn(handle, 0, [], key);
  }
  if (useFocusOut) {
    handle.dispatchEvent(new CustomEvent('focusout'));
  } else {
    keyUpOn(handle, 0, [], key);
  }
  await microtasksFinished();
}

function verifyFinishTextAnnotationMessage(expectedAnnotation: TextAnnotation) {
  const message = mockPlugin.findMessage('finishTextAnnotation');
  chrome.test.assertTrue(message !== undefined);
  chrome.test.assertEq('finishTextAnnotation', message.type);
  assertDeepEquals(expectedAnnotation, message.data);
}

chrome.test.runTests([
  // Test drawing the box based on data from the manager.
  async function testDrawsBox() {
    // Initial state. Textbox is not visible because it hasn't received an
    // initialize-text-box event yet.
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    // Create a 160x40 box at 80, 120.
    initializeBox(160, 40, 80, 120);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '160px', '40px', '80px', '120px');
    chrome.test.assertEq('Sample Text', textbox.$.textbox.value);

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
        'Arial, sans-serif', textboxStyles.getPropertyValue('font-family'));
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

    // Reset everything for later tests.
    manager.setTextTypeface(TextTypeface.SANS_SERIF);
    manager.setTextSize(12);
    manager.setTextStyles({
      [TextStyle.BOLD]: false,
      [TextStyle.ITALIC]: false,
    });
    manager.setTextColor(hexToColor(TEXT_COLORS[0]!.color));
    manager.setTextAlignment(TextAlignment.LEFT);
    await microtasksFinished();
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
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 30, y: 1.5, width: 45, height: 45},
        zoom: 0.5,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '50px', '50px', '230px', '151.5px');
    chrome.test.assertEq(
        '6px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 2.0. This also comes with x and y changes
    // simulating production.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 10, y: 6, width: 180, height: 180},
        zoom: 2.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '200px', '200px', '810px', '606px');
    chrome.test.assertEq(
        '24px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a scroll + resetting zoom to 1.0.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 100, y: 100, width: 90, height: 90},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '500px', '400px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where start of page is no longer in the viewport.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: -100, y: -100, width: 90, height: 90},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '300px', '200px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where textbox ends up off screen.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: -500, y: -500, width: 90, height: 90},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '-100px', '-200px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));
    chrome.test.succeed();
  },

  async function testViewportRotationChanges() {
    // Custom init with different x offsets to simulate a rectangular page with
    // rotations.
    function initializeBoxWithOrientation(
        width: number, height: number, x: number, y: number,
        orientation: number) {
      manager.dispatchEvent(new CustomEvent('initialize-text-box', {
        detail: {
          annotation: {
            text: '',
            textAttributes: {
              size: 12,
              typeface: TextTypeface.SANS_SERIF,
              styles: {
                [TextStyle.BOLD]: false,
                [TextStyle.ITALIC]: false,
              },
              alignment: TextAlignment.LEFT,
              color: hexToColor(TEXT_COLORS[0]!.color),
            },
            textBoxRect: {height, locationX: x, locationY: y, width},
            textOrientation: orientation,
            id: 0,
            pageNumber: 0,
          },
          pageCoordinates: orientation % 2 === 0 ? {x: 15, y: 3} : {x: 5, y: 3},
        },
      }));
    }

    // Helper to update the viewport to the specified number of clockwise
    // rotations.
    function updateViewportWithClockwiseRotations(rotations: number):
        Promise<void> {
      // Simulating real viewport changes. The x offset reduces when the
      // page is flipped horizontally, since it takes the whole window.
      // width and height flip when the page is horizontal.
      const x = rotations % 2 === 0 ? 15 : 5;
      const width = rotations % 2 === 0 ? 80 : 100;
      const height = rotations % 2 === 0 ? 100 : 80;
      manager.dispatchEvent(new CustomEvent('viewport-changed', {
        detail: {
          clockwiseRotations: rotations,
          pageDimensions: {x, y: 3, width, height},
          zoom: 1.0,
        },
      }));
      return microtasksFinished();
    }

    // Helper to check that the textbox styles match the expected rotation of
    // the text (in number of 90 degree clockwise rotations).
    function assertTextboxStyles(expectedTextRotation: number) {
      const expectedTransform =
          expectedTextRotation === 2 ? 'matrix(-1, 0, 0, -1, 0, 0)' : 'none';
      let expectedWritingMode = 'horizontal-tb';
      if (expectedTextRotation === 1) {
        expectedWritingMode = 'vertical-rl';
      } else if (expectedTextRotation === 3) {
        expectedWritingMode = 'sideways-lr';
      }
      const styles = getComputedStyle(textbox.$.textbox);
      chrome.test.assertEq(
          expectedTransform, styles.getPropertyValue('transform'));
      chrome.test.assertEq(
          expectedWritingMode, styles.getPropertyValue('writing-mode'));
    }

    // Initialize to a 50x48 box at 20, 30 + page offsets. Make box rotated
    // by 90 degrees clockwise compared to the PDF. This happens when the
    // viewport is rotated by 90 degrees CCW and the user creates a new
    // annotation, so simulate that scenario here.
    await updateViewportWithClockwiseRotations(3);
    initializeBoxWithOrientation(50, 48, 25, 33, 1);
    await microtasksFinished();
    // Position and size are in viewport coordinates, so the box is 50x48 in
    // the rotated viewport.
    assertPositionAndSize(textbox, '50px', '48px', '25px', '33px');
    // Textbox is non-rotated relative to the current viewport orientation.
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(0);
    assertPositionAndSize(textbox, '48px', '50px', '17px', '23px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(1);
    assertPositionAndSize(textbox, '50px', '48px', '35px', '5px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(2);
    assertPositionAndSize(textbox, '48px', '50px', '45px', '33px');
    assertTextboxStyles(3);

    // Back to the original position, size and style since we've now rotated
    // all the way around.
    await updateViewportWithClockwiseRotations(3);
    assertPositionAndSize(textbox, '50px', '48px', '25px', '33px');
    assertTextboxStyles(0);

    // Now initialize a box with no rotation relative to the PDF, at the same
    // location. This happens when the viewport has no rotation when the box is
    // created.
    await updateViewportWithClockwiseRotations(0);
    initializeBoxWithOrientation(50, 48, 35, 33, 0);
    await microtasksFinished();
    assertPositionAndSize(textbox, '50px', '48px', '35px', '33px');
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(1);
    assertPositionAndSize(textbox, '48px', '50px', '27px', '23px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(2);
    assertPositionAndSize(textbox, '50px', '48px', '25px', '25px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(3);
    assertPositionAndSize(textbox, '48px', '50px', '35px', '13px');
    assertTextboxStyles(3);

    // Back to 0 rotation should get us back to the original location and style.
    await updateViewportWithClockwiseRotations(0);
    assertPositionAndSize(textbox, '50px', '48px', '35px', '33px');
    assertTextboxStyles(0);

    chrome.test.succeed();
  },

  async function testCommit() {
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
    chrome.test.assertTrue(isVisible(textbox));
    await microtasksFinished();
    // Reset viewport to less offset page values and a non-1.0 zoom to validate
    // coordinate conversion.
    viewport.setZoom(2.0);
    await microtasksFinished();

    // With no edits, starting a new box just deletes the existing one; the
    // plugin won't get a message.
    mockPlugin.clearMessages();
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Editing text --> commit annotation on event.
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    const testAnnotation = getDefaultAnnotation();
    // Messages to the backend are in page coordinates.
    testAnnotation
        .textBoxRect = {locationX: 195, locationY: 147, height: 50, width: 50};

    function startNewAnnotationAndVerifyMessage(existing: boolean = false) {
      mockPlugin.clearMessages();
      initializeBox(100, 100, 400, 300, existing);
      verifyFinishTextAnnotationMessage(testAnnotation);
    }

    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    startNewAnnotationAndVerifyMessage();
    await microtasksFinished();
    // Reset expectation.
    testAnnotation.text = 'Sample Text';

    // Moving (or resizing) the box is an edit.
    chrome.test.assertTrue(isVisible(textbox));
    await dragHandle(textbox, 100, 100);
    // Adjust expectations for new box. Text is reset.
    // At 2x zoom, a 100px move in screen coordinates is a 50px move in page
    // coordinates.
    testAnnotation
        .textBoxRect = {height: 50, width: 50, locationX: 245, locationY: 197};
    startNewAnnotationAndVerifyMessage();
    await microtasksFinished();
    // Reset expectation.
    testAnnotation
        .textBoxRect = {height: 50, width: 50, locationX: 195, locationY: 147};

    // Any modifications to font are an edit.
    chrome.test.assertTrue(isVisible(textbox));
    manager.setTextTypeface(TextTypeface.MONOSPACE);
    await microtasksFinished();
    testAnnotation.textAttributes.typeface = TextTypeface.MONOSPACE;
    startNewAnnotationAndVerifyMessage();
    await microtasksFinished();
    // Reset expectation.
    testAnnotation.textAttributes.typeface = TextTypeface.SANS_SERIF;

    // If all the text is deleted, there is also no commit message.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = '';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    mockPlugin.clearMessages();
    // Initialize an existing box to set up the next test.
    initializeBox(100, 100, 400, 300, true);
    await microtasksFinished();
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // If we are editing an existing box, a finish message should be sent
    // regardless of edits or text.
    chrome.test.assertTrue(isVisible(textbox));
    testAnnotation.text = 'Hello World';
    startNewAnnotationAndVerifyMessage(/* existing= */ true);
    await microtasksFinished();

    // Existing box, text cleared.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = '';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    testAnnotation.text = '';
    startNewAnnotationAndVerifyMessage(/* existing= */ true);
    await microtasksFinished();

    // Message should also be sent if the element is disconnected.
    chrome.test.assertTrue(isVisible(textbox));
    testAnnotation.text = 'Hello World';
    await microtasksFinished();
    mockPlugin.clearMessages();
    // This happens if the user changes annotation mode.
    textbox.remove();
    const message = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertTrue(message !== undefined);
    chrome.test.assertEq('finishTextAnnotation', message.type);
    assertDeepEquals(testAnnotation, message.data);

    // Reset for future tests.
    document.body.appendChild(textbox);

    chrome.test.succeed();
  },

  async function testResizeWithKeyboard() {
    // Initialize to a 100x200 box at 400, 300.
    initializeBox(100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '100px', '200px', '400px', '300px');

    // 5px up and left.
    const topLeft = getRequiredElement(textbox, '.handle.top.left');
    await dragHandleWithKeyboard(topLeft, 'ArrowUp', 5);
    await dragHandleWithKeyboard(topLeft, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '105px', '205px', '395px', '295px');

    // Top handle 3px up and left. Left arrow is ignored on this handle.
    const top = getRequiredElement(textbox, '.handle.top.center');
    await dragHandleWithKeyboard(top, 'ArrowUp', 3);
    await dragHandleWithKeyboard(top, 'ArrowLeft', 3);
    assertPositionAndSize(textbox, '105px', '208px', '395px', '292px');

    // Top right handle 4px down and right -> makes box shorter and wider.
    // Use focusout event to finish right arrow drag to ensure it works.
    const topRight = getRequiredElement(textbox, '.handle.top.right');
    await dragHandleWithKeyboard(topRight, 'ArrowDown', 4);
    await dragHandleWithKeyboard(topRight, 'ArrowRight', 4, true);
    assertPositionAndSize(textbox, '109px', '204px', '395px', '296px');

    // Drag the left handle right and up. Upward motion is ignored. Left motion
    // makes the box 2px narrower.
    const left = getRequiredElement(textbox, '.handle.left.center');
    await dragHandleWithKeyboard(left, 'ArrowUp', 2);
    await dragHandleWithKeyboard(left, 'ArrowRight', 2);
    assertPositionAndSize(textbox, '107px', '204px', '397px', '296px');

    // Drag the right handle right and down. Downward motion is ignored. Right
    // motion makes the box 3px wider.
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandleWithKeyboard(right, 'ArrowDown', 3);
    await dragHandleWithKeyboard(right, 'ArrowRight', 3);
    assertPositionAndSize(textbox, '110px', '204px', '397px', '296px');

    // Drag the bottom left handle down and left to make the box 10px bigger
    // in both dimensions.
    const bottomLeft = getRequiredElement(textbox, '.handle.bottom.left');
    await dragHandleWithKeyboard(bottomLeft, 'ArrowDown', 10);
    await dragHandleWithKeyboard(bottomLeft, 'ArrowLeft', 10);
    assertPositionAndSize(textbox, '120px', '214px', '387px', '296px');

    // Drag the bottom handle down and left to make the box 5px taller.
    // Motion left is ignored.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandleWithKeyboard(bottom, 'ArrowDown', 5);
    await dragHandleWithKeyboard(bottom, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '120px', '219px', '387px', '296px');

    // Drag the bottom right handle down and right to make the box 2px bigger
    // in both dimensions.
    const bottomRight = getRequiredElement(textbox, '.handle.bottom.right');
    await dragHandleWithKeyboard(bottomRight, 'ArrowDown', 2);
    await dragHandleWithKeyboard(bottomRight, 'ArrowRight', 2);
    assertPositionAndSize(textbox, '122px', '221px', '387px', '296px');

    // Drag the bottom right handle up and left to try to make the box too
    // small. Make sure it clamps at the same minimum size, anchored on the top
    // left corner.
    await dragHandleWithKeyboard(bottomRight, 'ArrowUp', 100);
    await dragHandleWithKeyboard(bottomRight, 'ArrowLeft', 100);
    const clampedWidth = textbox.$.textbox.clientWidth;
    const clampedHeight = textbox.$.textbox.clientHeight;
    chrome.test.assertTrue(clampedHeight >= textbox.$.textbox.scrollHeight);
    // Min width is 36px.
    chrome.test.assertEq(36, clampedWidth);
    assertPositionAndSize(
        textbox, '36px', `${clampedHeight}px`, '387px', '296px');

    chrome.test.succeed();
  },

  async function testMoveWithKeyboard() {
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '100px', '100px', '400px', '300px');
    await dragHandleWithKeyboard(textbox, 'ArrowUp', 5);
    assertPositionAndSize(textbox, '100px', '100px', '400px', '295px');
    await dragHandleWithKeyboard(textbox, 'ArrowDown', 5);
    assertPositionAndSize(textbox, '100px', '100px', '400px', '300px');
    await dragHandleWithKeyboard(textbox, 'ArrowRight', 5);
    assertPositionAndSize(textbox, '100px', '100px', '405px', '300px');
    await dragHandleWithKeyboard(textbox, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '100px', '100px', '400px', '300px');

    // Make sure that arrow keys in the textarea are ignored.
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowUp', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowDown', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowLeft', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowRight', 1);
    assertPositionAndSize(textbox, '100px', '100px', '400px', '300px');
    chrome.test.succeed();
  },

  async function testEscapeAndDelete() {
    viewport.setZoom(1.0);
    // Initialize to a 100x100 box at 10, 10. Place the box in the top corner
    // so that the viewport won't scroll when it is focused.
    initializeBox(100, 100, 10, 10);
    // Wait for focus to happen so that we can correctly test focus changes
    // later.
    await eventToPromise('textbox-focused-for-test', textbox);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));

    // Editing text --> commit annotation on event.
    const testAnnotation = getDefaultAnnotation();
    // Messages to the backend are in page coordinates. Convert to page
    // coordinates since this is used for validating the commit message.
    testAnnotation
        .textBoxRect = {locationX: 0, locationY: 7, height: 100, width: 100};

    mockPlugin.clearMessages();
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    // Escape on the textarea blurs the textarea to focus the top level box.
    // This won't actually fire a focus event to the textbox since focus stays
    // within the box, so we wait for the blur event instead.
    const whenBlurred = eventToPromise('blur', textbox.$.textbox);
    keyDownOn(textbox.$.textbox, 0, [], 'Escape');
    await whenBlurred;
    // Textbox is still visible, because this event does not commit the
    // annotation.
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Escape on the textbox commits the annotation and hides the box.
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(testAnnotation);

    // If the user is dragging, escape commits the annotation at the start
    // location and hides the box.
    initializeBox(100, 100, 10, 10);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    mockPlugin.clearMessages();
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    const handle = getRequiredElement(textbox, '.handle.bottom.right');
    handle.dispatchEvent(new PointerEvent(
        'pointerdown', {composed: true, pointerId: 1, clientX: 0, clientY: 0}));
    handle.dispatchEvent(new PointerEvent(
        'pointerdown',
        {composed: true, pointerId: 1, clientX: 10, clientY: 10}));
    await microtasksFinished();
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    // Message is identical to before because 'pointerup' was never fired.
    verifyFinishTextAnnotationMessage(testAnnotation);

    // Escape without any modification hides the box but doesn't send a message.
    initializeBox(100, 100, 10, 10);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    mockPlugin.clearMessages();
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('setTextAnnotation'));

    // Initialize to a 100x100 box at 400, 300 with some text content. Use
    // "Delete" to clear all the content, which will trigger a message since
    // this is for an existing annotation.
    initializeBox(100, 100, 10, 10, true);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    mockPlugin.clearMessages();
    keyDownOn(textbox, 0, [], 'Delete');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    testAnnotation.text = '';
    verifyFinishTextAnnotationMessage(testAnnotation);

    chrome.test.succeed();
  },

  async function testCloseAndEvents() {
    let textBoxStates: TextBoxState[] = [];
    textbox.addEventListener('state-changed', e => {
      textBoxStates.push((e as CustomEvent<TextBoxState>).detail);
    });

    let finishInkStrokeModifiedEvents = 0;
    let finishInkStrokeUnmodifiedEvents = 0;
    PluginController.getInstance().getEventTarget().addEventListener(
        PluginControllerEventType.FINISH_INK_STROKE, e => {
          if ((e as CustomEvent<boolean>).detail) {
            finishInkStrokeModifiedEvents++;
          } else {
            finishInkStrokeUnmodifiedEvents++;
          }
        });

    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);

    // When a new box has no edits, commitTextAnnotation() will not trigger a
    // plugin message or a PluginControllerEventType.FINISH_INK_STROKE event.
    mockPlugin.clearMessages();
    textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));
    chrome.test.assertEq(0, finishInkStrokeModifiedEvents);
    chrome.test.assertEq(0, finishInkStrokeUnmodifiedEvents);
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);

    // When text is edited, commitTextAnnotation() will trigger a plugin message
    // and a PluginControllerEventType.FINISH_INK_STROKE modified event.
    textBoxStates = [];
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    assertDeepEquals([TextBoxState.NEW, TextBoxState.EDITED], textBoxStates);

    textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertTrue(
        mockPlugin.findMessage('finishTextAnnotation') !== undefined);
    chrome.test.assertEq(1, finishInkStrokeModifiedEvents);
    chrome.test.assertEq(0, finishInkStrokeUnmodifiedEvents);
    assertDeepEquals(
        [TextBoxState.NEW, TextBoxState.EDITED, TextBoxState.INACTIVE],
        textBoxStates);

    // When existing text is not edited, commitTextAnnotation() will trigger a
    // plugin message and a PluginControllerEventType.FINISH_INK_STROKE
    // unmodified event.
    textBoxStates = [];
    initializeBox(100, 100, 400, 300, true);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);
    textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertTrue(
        mockPlugin.findMessage('finishTextAnnotation') !== undefined);
    chrome.test.assertEq(1, finishInkStrokeModifiedEvents);
    chrome.test.assertEq(1, finishInkStrokeUnmodifiedEvents);
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);

    chrome.test.succeed();
  },

  async function testMoveViewportOnFocus() {
    // Ensure the viewport is scrollable by zooming in. Also ensure it is
    // located top/left, where we expect it.
    viewport.setZoom(2.0);
    viewport.goToPageAndXy(0, 0, 0);

    // Using manager initialization to get correct coordinates for the zoom
    // level.
    manager.initializeTextAnnotation({x: 20, y: 20});
    await eventToPromise('textbox-focused-for-test', textbox);
    await microtasksFinished();
    const styles = getComputedStyle(textbox);
    chrome.test.assertEq('20px', styles.getPropertyValue('left'));
    chrome.test.assertEq('20px', styles.getPropertyValue('top'));

    // Scroll away from the textbox. Note this method accepts page coordinates.
    // Scrolling by 35 in page coordinates scrolls by 70 in screen coordinates
    // at 2x zoom. Blurring the textbox in case it is still holding focus, to
    // simulate how scroll would work if the user scrolled by clicking on the
    // scrollbars, or by moving focus to the plugin and scrolling with the
    // keyboard. This also ensures the textbox gets a focus event when focused
    // later.
    manager.dispatchEvent(new CustomEvent('blur-text-box'));
    viewport.goToPageAndXy(0, 35, 35);
    await microtasksFinished();
    chrome.test.assertEq('-50px', styles.getPropertyValue('left'));
    chrome.test.assertEq('-50px', styles.getPropertyValue('top'));

    // Focus the textbox, which should cause the manager to scroll the viewport.
    // This won't actually scroll the viewport in the test, since the plugin
    // won't send a corresponding scroll message back.
    mockPlugin.clearMessages();
    textbox.focus();
    const syncScrollMessage = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // The box is at 20, 20 in viewport coordinates, and the viewport is 100px
    // wide. The manager specifies a margin of 10% of the viewport when
    // scrolling.
    chrome.test.assertEq(10, syncScrollMessage.x);
    chrome.test.assertEq(10, syncScrollMessage.y);

    chrome.test.succeed();
  },
]);
