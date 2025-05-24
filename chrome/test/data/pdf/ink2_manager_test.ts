// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, TextAnnotation, TextAttributes, TextBoxInit} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, DEFAULT_TEXTBOX_HEIGHT, DEFAULT_TEXTBOX_WIDTH, Ink2Manager, PluginController, PluginControllerEventType, TextAlignment, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertDeepEquals, MockDocumentDimensions, setGetAnnotationBrushReply, setUpInkTestContext} from './test_util.js';

const {viewport, mockPlugin, mockWindow} = setUpInkTestContext();
const manager = Ink2Manager.getInstance();

function getTestAnnotation(id: number): TextAnnotation {
  return {
    textAttributes: {
      typeface: TextTypeface.SANS_SERIF,
      size: 12,
      color: {r: 0, g: 100, b: 0},
      alignment: TextAlignment.LEFT,
      styles: {
        bold: false,
        italic: false,
      },
    },
    text: 'Hello World',
    id: id,
    pageNumber: 0,
    textBoxRect: {
      height: 35,
      locationX: 20,
      locationY: 25,
      width: 50,
    },
    textOrientation: 0,
  };
}

// Verifies that the plugin received a startTextAnnotation message for
// annotation with id 0.
function verifyStartTextAnnotationMessage(expected: boolean, id: number = 0) {
  const startTextAnnotationMessage =
      mockPlugin.findMessage('startTextAnnotation');
  chrome.test.assertEq(expected, startTextAnnotationMessage !== undefined);
  if (expected) {
    chrome.test.assertEq(
        'startTextAnnotation', startTextAnnotationMessage.type);
    chrome.test.assertEq(id, startTextAnnotationMessage.data);
  }
}

// Simulates the way the viewport is rotated from the plugin by setting updated
// DocumentDimensions. Assumes a non-rotated pageWidth of 80 and pageHeight of
// 100.
function rotateViewport(orientation: number) {
  const rotatedDocumentDimensions = new MockDocumentDimensions(0, 0);
  // When the plugin notifies the viewport of new dimensions for a rotation,
  // it swaps the width and height if the page is oriented sideways.
  if (orientation === 0 || orientation === 2) {
    rotatedDocumentDimensions.addPage(80, 100);
  } else {
    rotatedDocumentDimensions.addPage(100, 80);
  }
  rotatedDocumentDimensions.layoutOptions = {
    defaultPageOrientation: orientation,  // 90 degree CCW rotation
    direction: 2,                         // LTR
    twoUpViewEnabled: false,
  };
  viewport.setDocumentDimensions(rotatedDocumentDimensions);
}

chrome.test.runTests([
  async function testInitializeBrush() {
    chrome.test.assertFalse(manager.isInitializationStarted());
    chrome.test.assertFalse(manager.isInitializationComplete());

    // Initialize the brush.
    const brushPromise = manager.initializeBrush();
    chrome.test.assertTrue(manager.isInitializationStarted());
    await brushPromise;
    chrome.test.assertTrue(manager.isInitializationComplete());

    // Check that the manager requested the current annotation brush.
    const getAnnotationBrushMessage =
        mockPlugin.findMessage('getAnnotationBrush');
    chrome.test.assertTrue(getAnnotationBrushMessage !== undefined);
    chrome.test.assertEq('getAnnotationBrush', getAnnotationBrushMessage.type);

    // Check that the manager's brush is the one reported by the plugin.
    const brush = manager.getCurrentBrush();

    // Defaults set in `setupTestMockPluginForInk()`.
    chrome.test.assertEq(AnnotationBrushType.PEN, brush.type);
    assert(brush.color);
    chrome.test.assertEq(0, brush.color.r);
    chrome.test.assertEq(0, brush.color.g);
    chrome.test.assertEq(0, brush.color.b);
    chrome.test.assertEq(3, brush.size);

    chrome.test.succeed();
  },

  async function testSetBrushProperties() {
    const brushUpdates: AnnotationBrush[] = [];
    manager.addEventListener('brush-changed', e => {
      brushUpdates.push((e as CustomEvent<AnnotationBrush>).detail);
    });

    function assertBrushUpdate(index: number, expected: AnnotationBrush) {
      chrome.test.assertEq(index + 1, brushUpdates.length);
      chrome.test.assertEq(expected.type, brushUpdates[index]!.type);
      if (expected.color) {
        assertDeepEquals(expected.color, brushUpdates[index]!.color);
      }
      chrome.test.assertEq(expected.size, brushUpdates[index]!.size);
    }

    // Set "yellow 1" pen color and ensure the plugin is updated and the update
    // event is fired.
    const yellow1 = {r: 253, g: 214, b: 99};
    let expectedBrush = {
      type: AnnotationBrushType.PEN,
      color: yellow1,
      size: 3,
    };
    manager.setBrushColor(yellow1);
    assertAnnotationBrush(mockPlugin, expectedBrush);
    assertBrushUpdate(0, expectedBrush);

    // Set size to 1 and ensure the plugin is updated and the update event is
    // fired.
    manager.setBrushSize(1);
    expectedBrush.size = 1;
    assertAnnotationBrush(mockPlugin, expectedBrush);
    assertBrushUpdate(1, expectedBrush);

    // Set the highlighter and ensure plugin is updated and update event is
    // fired.
    const lightRed = {r: 242, g: 139, b: 130};
    expectedBrush = {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: lightRed,
      size: 8,
    };
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.HIGHLIGHTER, /*size=*/ 8, lightRed);
    await manager.setBrushType(AnnotationBrushType.HIGHLIGHTER);
    // Should have set the highlighter brush with the parameters from the
    // mock plugin.
    assertAnnotationBrush(mockPlugin, expectedBrush);
    assertBrushUpdate(2, expectedBrush);

    chrome.test.succeed();
  },

  async function testInitializeText() {
    chrome.test.assertFalse(manager.isTextInitializationComplete());

    // Initialize text annotation mode.
    const textPromise = manager.initializeTextAnnotations();
    await textPromise;
    chrome.test.assertTrue(manager.isTextInitializationComplete());

    // Check that the manager requested all the text annotations.
    const getAllTextAnnotationsMessage =
        mockPlugin.findMessage('getAllTextAnnotations');
    chrome.test.assertTrue(getAllTextAnnotationsMessage !== undefined);
    chrome.test.assertEq(
        'getAllTextAnnotations', getAllTextAnnotationsMessage.type);

    chrome.test.succeed();
  },

  async function testInitializeTextNonEmpty() {
    // Create a new Ink2Manager so that the state is separate from the rest of
    // the tests.
    const testManager = new Ink2Manager();
    testManager.setViewport(viewport);
    testManager.viewportChanged();

    // Set the reply to getAllTextAnnotations to return non-empty.
    const testAnnotation1 = getTestAnnotation(0);
    const testAnnotation2 = getTestAnnotation(1);
    testAnnotation2.text = 'Goodbye Moon';
    testAnnotation2.textBoxRect = {
      height: 25,
      locationX: 10,
      locationY: 65,
      width: 50,
    };
    mockPlugin.clearMessages();
    mockPlugin.setMessageReply('getAllTextAnnotations', {
      annotations: [testAnnotation1, testAnnotation2],
    });

    chrome.test.assertFalse(testManager.isTextInitializationComplete());
    await testManager.initializeTextAnnotations();
    chrome.test.assertTrue(testManager.isTextInitializationComplete());

    // Check that the manager requested all the text annotations.
    const getAllTextAnnotationsMessage =
        mockPlugin.findMessage('getAllTextAnnotations');
    chrome.test.assertTrue(getAllTextAnnotationsMessage !== undefined);
    chrome.test.assertEq(
        'getAllTextAnnotations', getAllTextAnnotationsMessage.type);

    // Check that initializing a new annotation in a different location sets
    // a different id.
    let whenInitEvent = eventToPromise('initialize-text-box', testManager);
    testManager.initializeTextAnnotation({x: 75, y: 20});
    let initEvent = await whenInitEvent;
    chrome.test.assertEq(2, initEvent.detail.annotation.id);
    chrome.test.assertEq('', initEvent.detail.annotation.text);
    verifyStartTextAnnotationMessage(false);

    // Check that the two existing annotations can be activated.
    mockPlugin.clearMessages();
    whenInitEvent = eventToPromise('initialize-text-box', testManager);
    testManager.initializeTextAnnotation({x: 30, y: 30});
    initEvent = await whenInitEvent;
    const testAnnotation1ScreenCoords = structuredClone(testAnnotation1);
    // Add page offsets. These are the defaults for the test viewport setup
    // of a 90x90 page in a 100x100 window.
    testAnnotation1ScreenCoords.textBoxRect.locationX =
        testAnnotation1.textBoxRect.locationX + 10;
    testAnnotation1ScreenCoords.textBoxRect.locationY =
        testAnnotation1.textBoxRect.locationY + 3;
    assertDeepEquals(testAnnotation1ScreenCoords, initEvent.detail.annotation);
    verifyStartTextAnnotationMessage(true, testAnnotation1.id);

    mockPlugin.clearMessages();
    whenInitEvent = eventToPromise('initialize-text-box', testManager);
    testManager.initializeTextAnnotation({x: 30, y: 70});
    initEvent = await whenInitEvent;
    const testAnnotation2ScreenCoords = structuredClone(testAnnotation2);
    testAnnotation2ScreenCoords.textBoxRect.locationX =
        testAnnotation2.textBoxRect.locationX + 10;
    testAnnotation2ScreenCoords.textBoxRect.locationY =
        testAnnotation2.textBoxRect.locationY + 3;
    assertDeepEquals(testAnnotation2ScreenCoords, initEvent.detail.annotation);
    verifyStartTextAnnotationMessage(true, testAnnotation2.id);

    mockPlugin.clearMessages();
    chrome.test.succeed();
  },

  function testSetFontProperties() {
    const fontUpdates: TextAttributes[] = [];
    manager.addEventListener('attributes-changed', e => {
      fontUpdates.push((e as CustomEvent<TextAttributes>).detail);
    });

    function assertTextUpdate(index: number, expected: TextAttributes) {
      chrome.test.assertEq(index + 1, fontUpdates.length);
      chrome.test.assertEq(expected.typeface, fontUpdates[index]!.typeface);
      chrome.test.assertEq(expected.size, fontUpdates[index]!.size);
      assertDeepEquals(expected.color, fontUpdates[index]!.color);
      chrome.test.assertEq(expected.alignment, fontUpdates[index]!.alignment);
      assertDeepEquals(expected.styles, fontUpdates[index]!.styles);
    }

    // Update font. Note the other `expectedAttributes` values come from the
    // defaults set in ink2_manager.ts.
    manager.setTextTypeface(TextTypeface.SERIF);
    const expectedAttributes = {
      typeface: TextTypeface.SERIF,
      size: 12,
      color: {r: 0, g: 0, b: 0},
      alignment: TextAlignment.LEFT,
      styles: {
        bold: false,
        italic: false,
      },
    };
    assertTextUpdate(0, expectedAttributes);

    // Set size to 10.
    manager.setTextSize(10);
    expectedAttributes.size = 10;
    assertTextUpdate(1, expectedAttributes);

    // Set alignment to CENTER.
    manager.setTextAlignment(TextAlignment.CENTER);
    expectedAttributes.alignment = TextAlignment.CENTER;
    assertTextUpdate(2, expectedAttributes);

    // Set color to blue.
    const blue = {r: 0, b: 100, g: 0};
    manager.setTextColor(blue);
    expectedAttributes.color = blue;
    assertTextUpdate(3, expectedAttributes);

    // Set style to bold + italic.
    const boldItalic = {bold: true, italic: true};
    manager.setTextStyles(boldItalic);
    expectedAttributes.styles = boldItalic;
    assertTextUpdate(4, expectedAttributes);

    chrome.test.succeed();
  },

  async function testNoInitializeOnScrollbar() {
    let initEvents = 0;
    manager.addEventListener('initialize-text-box', () => {
      initEvents++;
    });

    // Should fire an event to blur any existing text box when click is on
    // a scrollbar.
    let blurEvents = 0;
    manager.addEventListener('blur-text-box', () => {
      blurEvents++;
    });

    // Zoom in to 2x so that there are scrollbars in both x and y.
    let whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(2.0);
    await whenViewportChanged;

    // Confirm both scrollbars and mock viewport dimensions.
    chrome.test.assertTrue(viewport.documentHasScrollbars().vertical);
    chrome.test.assertTrue(viewport.documentHasScrollbars().horizontal);
    chrome.test.assertEq(100, viewport.size.width);
    chrome.test.assertEq(100, viewport.size.height);
    chrome.test.assertFalse(viewport.scrollbarWidth === 0);

    const edge = 100 - viewport.scrollbarWidth;
    Ink2Manager.getInstance().initializeTextAnnotation({x: edge, y: 20});
    chrome.test.assertEq(1, blurEvents);
    Ink2Manager.getInstance().initializeTextAnnotation({x: 20, y: edge});
    chrome.test.assertEq(0, initEvents);
    chrome.test.assertEq(2, blurEvents);

    // Reset the zoom for the next test.
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(1.0);
    await whenViewportChanged;

    chrome.test.succeed();
  },

  async function testInitializeTextboxNoLocation() {
    // Update the viewport to be sufficiently large to accommodate a default
    // size textbox for testing.
    const documentDimensions = new MockDocumentDimensions(0, 0);
    documentDimensions.addPage(400, 500);
    viewport.setDocumentDimensions(documentDimensions);

    // Make the viewport window larger.
    mockWindow.setSize(500, 500);

    let whenInitEvent = eventToPromise('initialize-text-box', manager);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    manager.initializeTextAnnotation();
    let initEvent = await whenInitEvent;

    // The full document fits in the window.
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_HEIGHT, initEvent.detail.annotation.textBoxRect.height);
    // The page is centered on the viewport in the x direction, so it runs from
    // 55 to 445 after subtracting the 5px shadows on each side. The center is
    // at (55 + 445) / 2 = 250. Subtract half the default width to get the left
    // edge.
    chrome.test.assertEq(
        250 - DEFAULT_TEXTBOX_WIDTH / 2,
        initEvent.detail.annotation.textBoxRect.locationX);
    // The visible page starts at the y shadow/margin, which is 3, and is 490
    // tall after accounting for shadows. The center is at
    // (493 + 3) / 2 = 248. Subtract half the default height to get the top
    // edge.
    chrome.test.assertEq(
        248 - DEFAULT_TEXTBOX_HEIGHT / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageNumber);
    chrome.test.assertEq(55, initEvent.detail.pageCoordinates.x);
    chrome.test.assertEq(3, initEvent.detail.pageCoordinates.y);

    // Zoom to 2.0. Now, the new annotation should be centered on the visible
    // portion of the page.
    viewport.setZoom(2.0);
    whenInitEvent = eventToPromise('initialize-text-box', manager);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    manager.initializeTextAnnotation();
    initEvent = await whenInitEvent;

    chrome.test.assertEq(
        DEFAULT_TEXTBOX_HEIGHT, initEvent.detail.annotation.textBoxRect.height);
    // The page starts at the x shadow/margin, which is 10, and goes to the
    // viewport edge. The center is (500 + 10) / 2 = 255. Subtract half the
    // default width to get the left edge.
    chrome.test.assertEq(
        255 - DEFAULT_TEXTBOX_WIDTH / 2,
        initEvent.detail.annotation.textBoxRect.locationX);
    // The visible page starts at the y shadow/margin, which is 6, and goes to
    // the viewport edge. The center is (500 + 6) / 2 = 253. Subtract half the
    // default height to get the top edge.
    chrome.test.assertEq(
        253 - DEFAULT_TEXTBOX_HEIGHT / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageNumber);
    chrome.test.assertEq(10, initEvent.detail.pageCoordinates.x);
    chrome.test.assertEq(6, initEvent.detail.pageCoordinates.y);

    // Zoom to 0.5. The new box should still be centered on the page, even
    // though it is not centered in the viewport.
    viewport.setZoom(0.5);
    whenInitEvent = eventToPromise('initialize-text-box', manager);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    Ink2Manager.getInstance().initializeTextAnnotation();
    initEvent = await whenInitEvent;

    chrome.test.assertEq(
        DEFAULT_TEXTBOX_HEIGHT, initEvent.detail.annotation.textBoxRect.height);
    // The page is centered on the viewport in the x direction, so it runs from
    // 152.5 to 347.5. The center is at (152.5 + 347.5) / 2 = 250. Subtract half
    // the default width to get the left edge.
    chrome.test.assertEq(
        250 - DEFAULT_TEXTBOX_WIDTH / 2,
        initEvent.detail.annotation.textBoxRect.locationX);
    // The page starts at the y margin/shadow of 1.5 and runs to 246.5 since it
    // is 245 tall without the shadows. The center is at
    // (1.5 + 246.5) / 2 = 124. Subtract half the default height to get the top
    // edge.
    chrome.test.assertEq(
        124 - DEFAULT_TEXTBOX_HEIGHT / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageNumber);
    chrome.test.assertEq(152.5, initEvent.detail.pageCoordinates.x);
    chrome.test.assertEq(1.5, initEvent.detail.pageCoordinates.y);

    // Reset zoom, window size and annotation id for next test.
    viewport.setZoom(1.0);
    manager.resetAnnotationIdForTest();
    mockWindow.setSize(100, 100);

    chrome.test.succeed();
  },

  async function testInitializeTextBox() {
    // Create a new mock document dimensions that has different width from
    // height. This is relevant for testing different rotations.
    const documentDimensions = new MockDocumentDimensions(0, 0);
    documentDimensions.addPage(80, 100);
    viewport.setDocumentDimensions(documentDimensions);

    // Add listeners for the expected events that fire in response to an
    // initializeTextAnnotation call.
    let eventsDispatched: Array<{name: string, detail: any}> = [];
    ['initialize-text-box', 'attributes-changed'].forEach(eventName => {
      manager.addEventListener(eventName, e => {
        eventsDispatched.push(
            {name: eventName, detail: (e as CustomEvent).detail});
      });
    });

    const attributes = manager.getCurrentTextAttributes();
    async function verifyTextboxInit(
        x: number, y: number, rotation: number, id: number) {
      const whenUpdateEvent = eventToPromise('initialize-text-box', manager);
      Ink2Manager.getInstance().initializeTextAnnotation({x, y});
      await whenUpdateEvent;
      chrome.test.assertEq(2, eventsDispatched.length);
      chrome.test.assertEq('initialize-text-box', eventsDispatched[0]!.name);
      const initData = eventsDispatched[0]!.detail as TextBoxInit;
      chrome.test.assertEq('', initData.annotation.text);
      assertDeepEquals(attributes, initData.annotation.textAttributes);
      chrome.test.assertEq(
          DEFAULT_TEXTBOX_HEIGHT, initData.annotation.textBoxRect.height);
      chrome.test.assertEq(x, initData.annotation.textBoxRect.locationX);
      chrome.test.assertEq(y, initData.annotation.textBoxRect.locationY);
      chrome.test.assertEq(
          DEFAULT_TEXTBOX_WIDTH, initData.annotation.textBoxRect.width);
      chrome.test.assertEq(0, initData.annotation.pageNumber);
      chrome.test.assertEq(id, initData.annotation.id);
      chrome.test.assertEq(rotation, initData.annotation.textOrientation);
      // Placeholder viewport has a 80x100 page and 100x100 window.
      // The y offset is always 3px, because the page is always positioned
      // 3px from the top. When the page is oriented vertically, it is centered
      // in the viewport with an additional 5px margin in x, creating pageX =
      // (100 - 80)/2 + 5 = 15px offset. When the page is oriented horizontally,
      // it is as wide as the viewport, so it uses the minimum 5px margin for
      // pageX.
      chrome.test.assertEq(
          rotation % 2 === 0 ? 15 : 5, initData.pageCoordinates.x);
      chrome.test.assertEq(3, initData.pageCoordinates.y);
      chrome.test.assertEq('attributes-changed', eventsDispatched[1]!.name);
      assertDeepEquals(attributes, eventsDispatched[1]!.detail);
      eventsDispatched = [];

      // Since this is a new annotation, it shouldn't have sent a message to the
      // plugin.
      verifyStartTextAnnotationMessage(false);
    }

    // Test initialization in different positions and different viewport
    // rotations. id should increment with each new textbox.
    rotateViewport(/* clockwiseRotations= */ 3);
    await verifyTextboxInit(/* x= */ 15, /* y= */ 10, /* rotations= */ 1,
                            /* id= */ 0);
    rotateViewport(/* clockwiseRotations= */ 2);
    await verifyTextboxInit(/* x= */ 50, /* y= */ 60, /* rotations= */ 2,
                            /* id= */ 1);
    rotateViewport(/* clockwiseRotations= */ 1);
    await verifyTextboxInit(/* x= */ 80, /* y = */ 20, /* rotations= */ 3,
                            /* id= */ 2);
    rotateViewport(/* clockwiseRotations= */ 0);
    await verifyTextboxInit(/* x= */ 20, /* y= */ 23, /* rotations= */ 0,
                            /* id= */ 3);

    chrome.test.succeed();
  },

  function testCommitTextAnnotation() {
    function verifyFinishTextAnnotationMessage(
        annotationPageCoords: TextAnnotation) {
      const finishTextAnnotationMessage =
          mockPlugin.findMessage('finishTextAnnotation');
      chrome.test.assertTrue(finishTextAnnotationMessage !== undefined);
      chrome.test.assertEq(
          'finishTextAnnotation', finishTextAnnotationMessage.type);
      assertDeepEquals(annotationPageCoords, finishTextAnnotationMessage.data);
    }

    function testCommitAnnotation(
        annotationScreenCoords: TextAnnotation,
        annotationPageCoords: TextAnnotation) {
      // Listen for PluginControllerEventType.FINISH_INK_STROKE events. The
      // manager dispatches these on PluginController's eventTarget.
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

      // Committing with edited = true should fire a modified event.
      // Use structuredClone since the manager edits the object in place,
      // and we want to reuse this below.
      manager.commitTextAnnotation(
          structuredClone(annotationScreenCoords), true);
      chrome.test.assertEq(1, finishInkStrokeModifiedEvents);
      chrome.test.assertEq(0, finishInkStrokeUnmodifiedEvents);
      verifyFinishTextAnnotationMessage(annotationPageCoords);
      mockPlugin.clearMessages();

      // Committing with edited = false should fire an unmodified event.
      manager.commitTextAnnotation(
          structuredClone(annotationScreenCoords), false);
      chrome.test.assertEq(1, finishInkStrokeModifiedEvents);
      chrome.test.assertEq(1, finishInkStrokeUnmodifiedEvents);
      verifyFinishTextAnnotationMessage(annotationPageCoords);
      mockPlugin.clearMessages();
    }

    // Test committing annotations at different rotations to ensure the
    // conversion back to page coordinates works correctly. Note that the
    // page screen rectangle will be 70x90 since there are 10px of page
    // shadow.

    // 90 degrees CCW
    rotateViewport(/* clockwiseRotations= */ 3);
    let annotationScreenCoords = getTestAnnotation(3);
    let annotationPageCoords = getTestAnnotation(3);
    annotationPageCoords.textBoxRect = {
      height: 50,
      width: 35,
      locationX: 13,
      locationY: 15,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true);
    mockPlugin.clearMessages();

    // 180 degrees
    rotateViewport(/* clockwiseRotations= */ 2);
    annotationScreenCoords = getTestAnnotation(2);
    annotationPageCoords = getTestAnnotation(2);
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect = {
      height: 35,
      width: 50,
      locationX: 15,
      locationY: 33,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true);
    mockPlugin.clearMessages();

    // 90 degrees CW
    rotateViewport(/* clockwiseRotations= */ 1);
    annotationScreenCoords = getTestAnnotation(1);
    annotationPageCoords = getTestAnnotation(1);
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect = {
      height: 50,
      width: 35,
      locationX: 22,
      locationY: 25,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true);
    mockPlugin.clearMessages();

    // Normal orientation (0 degrees).
    rotateViewport(/* clockwiseRotations= */ 0);
    annotationScreenCoords = getTestAnnotation(0);
    annotationPageCoords = getTestAnnotation(0);
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect = {
      height: 35,
      width: 50,
      locationX: 5,
      locationY: 22,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Note: not deleting since we re-activate this annotation in the next
    // test.
    chrome.test.succeed();
  },

  async function testInitializeExistingAnnotation() {
    // Add listeners for the expected events that fire in response to an
    // initializeTextAnnotation message.
    const eventsDispatched: Array<{name: string, detail: any}> = [];
    ['initialize-text-box', 'attributes-changed'].forEach(eventName => {
      manager.addEventListener(eventName, e => {
        eventsDispatched.push(
            {name: eventName, detail: (e as CustomEvent).detail});
      });
    });

    const whenUpdateEvent = eventToPromise('initialize-text-box', manager);
    // Click inside the existing text box area.
    Ink2Manager.getInstance().initializeTextAnnotation({x: 40, y: 40});
    await whenUpdateEvent;
    chrome.test.assertEq(2, eventsDispatched.length);
    chrome.test.assertEq('initialize-text-box', eventsDispatched[0]!.name);
    const initData = eventsDispatched[0]!.detail as TextBoxInit;
    const testAnnotation = getTestAnnotation(0);
    assertDeepEquals(testAnnotation, initData.annotation);
    // Still using the 80x100 page from the previous test.
    chrome.test.assertEq(15, initData.pageCoordinates.x);
    chrome.test.assertEq(3, initData.pageCoordinates.y);
    chrome.test.assertEq('attributes-changed', eventsDispatched[1]!.name);
    assertDeepEquals(
        testAnnotation.textAttributes, eventsDispatched[1]!.detail);

    // Since this is an existing annotation, it should send a start message to
    // the plugin.
    verifyStartTextAnnotationMessage(true);
    chrome.test.succeed();
  },

  async function testViewport() {
    const initialParams = manager.getViewportParams();
    chrome.test.assertEq(1.0, initialParams.zoom);
    // pageMarginY * zoom = 3 * 1
    chrome.test.assertEq(3, initialParams.pageDimensions.y);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (100 - 80 * 1)/2 + 5 * 1
    chrome.test.assertEq(15, initialParams.pageDimensions.x);
    // 10px of width are taken up by PAGE_SHADOW.
    chrome.test.assertEq(70, initialParams.pageDimensions.width);
    // 20px of height are also taken up by PAGE_SHADOW.
    chrome.test.assertEq(90, initialParams.pageDimensions.height);
    chrome.test.assertEq(0, initialParams.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 20, 25 in screen coordinates. Make
    // sure clicking there creates the box, and clicking just outside of this
    // does not.
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 20, y: 25});
    verifyStartTextAnnotationMessage(true);
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 19, y: 24});
    verifyStartTextAnnotationMessage(false);

    // Zoom out should fire an event.
    let whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(0.5);
    let changedEvent = await whenViewportChanged;
    chrome.test.assertEq(0.5, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * .5
    chrome.test.assertEq(1.5, changedEvent.detail.pageDimensions.y);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (100 - 80 * .5)/2 + 5 * .5
    chrome.test.assertEq(32.5, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(35, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(45, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 35, 12.5 in screen coordinates.
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 35, y: 13});
    verifyStartTextAnnotationMessage(true);
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 34, y: 12});
    verifyStartTextAnnotationMessage(false);

    // Zoom in should fire an event.
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(2.0);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * 2
    chrome.test.assertEq(6, changedEvent.detail.pageDimensions.y);
    // docWidth * zoom > windowWidth, so this is now pageMarginX * zoom = 5 * 2
    chrome.test.assertEq(10, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(140, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(180, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 25, 50 in screen coordinates.
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 25, y: 50});
    verifyStartTextAnnotationMessage(true);
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 24, y: 49});
    verifyStartTextAnnotationMessage(false);

    // Translation.
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.goToPageAndXy(0, 20, 20);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-34, changedEvent.detail.pageDimensions.y);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-30, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(140, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(180, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at -15, 10 in screen coordinates.
    // It has width 100 and height 70 so (0, 81) should be just outside the box
    // and (0, 80) just inside.
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 0, y: 80});
    verifyStartTextAnnotationMessage(true);
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 0, y: 81});
    verifyStartTextAnnotationMessage(false);

    // Rotation
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    rotateViewport(/* clockwiseRotations= */ 3);  // 90 degree CCW rotation.
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    chrome.test.assertEq(-34, changedEvent.detail.pageDimensions.y);
    chrome.test.assertEq(-30, changedEvent.detail.pageDimensions.x);
    // Width and height are switched.
    chrome.test.assertEq(180, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(140, changedEvent.detail.pageDimensions.height);
    // Rotations now non-zero.
    chrome.test.assertEq(3, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 14, -4 in screen coordinates.
    // It has width 70 and height 100 so (85, 0) should be just outside the box
    // and (84, 0) just inside.
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 84, y: 0});
    verifyStartTextAnnotationMessage(true);
    mockPlugin.clearMessages();
    Ink2Manager.getInstance().initializeTextAnnotation({x: 85, y: 0});
    verifyStartTextAnnotationMessage(false);

    chrome.test.succeed();
  },

  function testTextboxFocused() {
    // Reset viewport to position 0, 0 and 1.0 zoom.
    viewport.setZoom(1.0);
    rotateViewport(/* clockwiseRotations= */ 0);  // 0 rotation.
    viewport.goToPageAndXy(0, 0, 0);

    // Simulate creating a textbox at (55, 50) (near center of the viewport).
    manager.initializeTextAnnotation({x: 55, y: 50});

    // Zoom by 2x. This would cause the textbox to be out of the view, at
    // location 110, 100.
    viewport.setZoom(2.0);

    mockPlugin.clearMessages();

    // Now simulate focus moving to the textbox (e.g., because the user tabbed
    // there). Make sure the manager sends a message to the plugin to scroll
    // the viewport to the textbox.
    manager.textBoxFocused({
      locationX: 110,
      locationY: 100,
      height: 50,
      width: 50,
    });

    let syncScrollMessage = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // The content is 160x200 (2x page size), and the viewport is 100x100. The
    // scrollbar is 5px wide.
    // Max scroll is contentSize - viewportSize + scrollbarWidth.
    // The max scroll is therefore 160 - 100 + 5 = 65 horizontally, so in the x
    // direction scroll is clamped at 65, which is less than the desired scroll
    // of 110 - .1 * viewportWidth = 100.
    // Vertically, the desired y position is 100 - .1 * viewportHeight = 90,
    // which is within the max scroll of
    // contentWidth - viewportWidth + scrollbarWidth = 105.
    chrome.test.assertEq(65, syncScrollMessage.x);
    chrome.test.assertEq(90, syncScrollMessage.y);

    // Focusing a textbox that is already in the view shouldn't scroll the
    // viewport.
    mockPlugin.clearMessages();
    manager.textBoxFocused({
      locationX: 45,
      locationY: 10,
      height: 50,
      width: 50,
    });
    syncScrollMessage = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertEq(undefined, syncScrollMessage);

    // Focus a textbox that is out of bounds the other direction.
    manager.textBoxFocused({
      locationX: -20,
      locationY: -10,
      height: 50,
      width: 50,
    });
    syncScrollMessage = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // Scroll x to maxScrollWidth - 20 - .1 * viewportWidth = 65 - 20 - 10 = 35.
    // Scroll y to maxScrollHeight - 10 - .1 * viewportHeight =
    // 90 - 10 - 10 = 70.
    chrome.test.assertEq(35, syncScrollMessage.x);
    chrome.test.assertEq(70, syncScrollMessage.y);

    chrome.test.succeed();
  },
]);
