// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, TEXT_COLORS, TextAlignment, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, initializeBox, setupTextBoxTest} from './ink2_text_box_test_utils.js';

const {manager, mockPlugin, textbox, viewport} = setupTextBoxTest();

chrome.test.runTests([
  async function testViewportChanges() {
    // Initialize to a 100x100 box at 410, 303.
    initializeBox(manager, 100, 100, 410, 303);
    await microtasksFinished();

    assertPositionAndSize(textbox, '124px', '120px', '393px', '288px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 0.5. This also comes with x and y changes
    // simulating production.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 30, y: 1.5, width: 500, height: 500},
        zoom: 0.5,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '74px', '70px', '213px', '136.5px');
    chrome.test.assertEq(
        '6px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a zoom change to 2.0. This also comes with x and y changes
    // simulating production.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 10, y: 6, width: 2000, height: 2000},
        zoom: 2.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '224px', '220px', '793px', '591px');
    chrome.test.assertEq(
        '24px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Simulate a scroll + resetting zoom to 1.0.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: 100, y: 100, width: 1000, height: 1000},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '483px', '385px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where start of page is no longer in the viewport.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: -100, y: -100, width: 1000, height: 1000},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '283px', '185px');
    chrome.test.assertEq(
        '12px',
        getComputedStyle(textbox.$.textbox).getPropertyValue('font-size'));

    // Scroll where textbox ends up off screen.
    manager.dispatchEvent(new CustomEvent('viewport-changed', {
      detail: {
        clockwiseRotations: 0,
        pageDimensions: {x: -500, y: -500, width: 1000, height: 1000},
        zoom: 1.0,
      },
    }));
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '-117px', '-215px');
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
            pageIndex: 0,
          },
          pageDimensions: orientation % 2 === 0 ? {x: 15, y: 3} : {x: 5, y: 3},
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
    assertPositionAndSize(textbox, '74px', '68px', '8px', '18px');
    // Textbox is non-rotated relative to the current viewport orientation.
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(0);
    assertPositionAndSize(textbox, '68px', '74px', '2px', '6px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(1);
    assertPositionAndSize(textbox, '74px', '68px', '18px', '-10px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(2);
    assertPositionAndSize(textbox, '68px', '74px', '30px', '16px');
    assertTextboxStyles(3);

    // Back to the original position, size and style since we've now rotated
    // all the way around.
    await updateViewportWithClockwiseRotations(3);
    assertPositionAndSize(textbox, '74px', '68px', '8px', '18px');
    assertTextboxStyles(0);

    // Now initialize a box with no rotation relative to the PDF, at the same
    // location. This happens when the viewport has no rotation when the box is
    // created.
    await updateViewportWithClockwiseRotations(0);
    initializeBoxWithOrientation(50, 48, 35, 33, 0);
    await microtasksFinished();
    assertPositionAndSize(textbox, '74px', '68px', '18px', '18px');
    assertTextboxStyles(0);

    await updateViewportWithClockwiseRotations(1);
    assertPositionAndSize(textbox, '68px', '74px', '12px', '6px');
    assertTextboxStyles(1);

    await updateViewportWithClockwiseRotations(2);
    assertPositionAndSize(textbox, '74px', '68px', '8px', '10px');
    assertTextboxStyles(2);

    await updateViewportWithClockwiseRotations(3);
    assertPositionAndSize(textbox, '68px', '74px', '20px', '-4px');
    assertTextboxStyles(3);

    // Back to 0 rotation should get us back to the original location and style.
    await updateViewportWithClockwiseRotations(0);
    assertPositionAndSize(textbox, '74px', '68px', '18px', '18px');
    assertTextboxStyles(0);

    chrome.test.succeed();
  },

  async function testMoveViewportOnFocus() {
    // Ensure the viewport is scrollable by zooming in. Also ensure it is
    // located top/left, where we expect it.
    viewport.setZoom(2.0);
    viewport.goToPageAndXy(0, 0, 0);

    // Using manager initialization to get correct coordinates for the zoom
    // level. A click position of 72 will offset the y location by
    // zoom * text size / 2 = text size = 12 by default. So this initializes
    // the top left corner to 60, 60.
    await manager.initializeTextAnnotation({x: 60, y: 72});
    await eventToPromise('textbox-focused-for-test', textbox);
    await microtasksFinished();
    const styles = getComputedStyle(textbox);
    chrome.test.assertEq('43px', styles.getPropertyValue('left'));
    chrome.test.assertEq('45px', styles.getPropertyValue('top'));

    // Scroll away from the textbox. Note this method accepts page coordinates.
    // Scrolling by 35 in page coordinates scrolls by 70 in screen coordinates
    // at 2x zoom. Blurring the textbox in case it is still holding focus, to
    // simulate how scroll would work if the user scrolled by clicking on the
    // scrollbars, or by moving focus to the plugin and scrolling with the
    // keyboard. This also ensures the textbox gets a focus event when focused
    // later.
    textbox.blur();
    viewport.goToPageAndXy(0, 35, 35);
    await microtasksFinished();
    chrome.test.assertEq('-27px', styles.getPropertyValue('left'));
    chrome.test.assertEq('-25px', styles.getPropertyValue('top'));

    // Focus the textbox, which should cause the manager to scroll the viewport.
    // This won't actually scroll the viewport in the test, since the plugin
    // won't send a corresponding scroll message back.
    mockPlugin.clearMessages();
    // Manually fire the focus event. Browser focus is not guaranteed in tests.
    textbox.focus();
    textbox.dispatchEvent(new FocusEvent('focus'));
    const syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // The box is at 60, 60 in viewport coordinates, and the viewport is 500px
    // wide. The manager specifies a margin of 10% of the viewport when
    // scrolling, so both of these end up at 10.
    chrome.test.assertEq(10, syncScrollMessage.x);
    chrome.test.assertEq(10, syncScrollMessage.y);

    chrome.test.succeed();
  },
]);
