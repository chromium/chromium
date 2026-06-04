// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, TextAnnotation, TextAnnotationMessageData, TextAttributes, TextBoxInit, UndoRedoStateChangedDetail, ViewportParams} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, DEFAULT_TEXTBOX_WIDTH, Ink2Manager, MIN_TEXTBOX_SIZE_PX, PluginController, PluginControllerEventType, TextAlignment, TextAnnotationSource, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertDeepEquals, MockDocumentDimensions, setGetAnnotationBrushReply, setUpInkTestContext} from './test_util.js';

const {viewport, mockPlugin} = setUpInkTestContext();
const manager = Ink2Manager.getInstance();

// Calls initializeTextAnnotation() with `point` as the location and mocks out
// behavior implemented by `ink-text-box` in production as follows:
// - Listens for initialize-text-box and deactivate-text-box events
// - Asserts that deactivate-text-box is only fired if `expectDeactivate` is
//   true.
// - If `annotationToCommit` is specified, calls commitTextAnnotation with
//   this annotation after receiving the deactivate-text-box-event.
// - Calls setTextBoxActive(false) in response to the deactivate-text-box event.
// - After receiving initialize-text-box, calls setTextBoxActive(true).
// Returns the initialize-text-box event.
async function changeActiveAnnotation(
    point: {x: number, y: number}|null, expectDeactivate: boolean,
    annotationToCommit?: TextAnnotation): Promise<CustomEvent<TextBoxInit>> {
  const whenInitEvent =
      eventToPromise<CustomEvent<TextBoxInit>>('initialize-text-box', manager);

  let deactivateFired = false;
  const deactivateListener = () => {
    deactivateFired = true;
    if (annotationToCommit) {
      manager.commitTextAnnotation(annotationToCommit, true, []);
    }
    manager.setTextBoxActive(false);
  };
  manager.addEventListener(
      'deactivate-text-box', deactivateListener, {once: true});

  const created = point ? await manager.initializeTextAnnotation(point) :
                          await manager.initializeTextAnnotation();
  chrome.test.assertTrue(created);

  const initEvent = await whenInitEvent;
  chrome.test.assertEq(expectDeactivate, deactivateFired);
  manager.removeEventListener('deactivate-text-box', deactivateListener);
  manager.setTextBoxActive(true);

  return initEvent;
}

function getTestAnnotation(id: number): TextAnnotation {
  return {
    id: id,
    mojoTextInfo: new ArrayBuffer(0),
    pageIndex: 0,
    pdfZoom: 1.0,
    text: 'Hello World',
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
    textBoxRect: {
      height: 35,
      locationX: 60,
      locationY: 25,
      width: 50,
    },
    textOrientation: 0,
  };
}

function getTestAnnotationMessageData(id: number): TextAnnotationMessageData {
  return {
    ...getTestAnnotation(id),
    isEdited: false,
    newTypefaces: [],
    source: TextAnnotationSource.USER,
  };
}

function resetFontProperties() {
  const textAttributes = getTestAnnotation(0).textAttributes;
  manager.setTextTypeface(textAttributes.typeface);
  manager.setTextSize(textAttributes.size);
  manager.setTextAlignment(textAttributes.alignment);
  manager.setTextColor(textAttributes.color);
  manager.setTextStyles(textAttributes.styles);
}

// Verifies that the plugin received a editTextAnnotation message for annotation
// with id 0.
function verifyEditTextAnnotationMessage(expected: boolean, id: number = 0) {
  const editTextAnnotationMessage =
      mockPlugin.findMessage<{type: string, data: number}>(
          'editTextAnnotation');
  chrome.test.assertEq(expected, editTextAnnotationMessage !== undefined);
  if (expected) {
    chrome.test.assertEq('editTextAnnotation', editTextAnnotationMessage!.type);
    chrome.test.assertEq(id, editTextAnnotationMessage!.data);
  }
}

function verifyFinishTextAnnotationMessage(
    expectedMessage: TextAnnotationMessageData) {
  const finishTextAnnotationMessage =
      mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
          'finishTextAnnotation');
  chrome.test.assertTrue(finishTextAnnotationMessage !== undefined);
  chrome.test.assertEq(
      'finishTextAnnotation', finishTextAnnotationMessage.type);
  assertDeepEquals(expectedMessage, finishTextAnnotationMessage.data);
}

// Simulates the way the viewport is rotated from the plugin by setting updated
// DocumentDimensions. Assumes a non-rotated pageWidth of 400 and pageHeight of
// 500.
function rotateViewport(orientation: number) {
  const rotatedDocumentDimensions = new MockDocumentDimensions(0, 0);
  // When the plugin notifies the viewport of new dimensions for a rotation,
  // it swaps the width and height if the page is oriented sideways.
  if (orientation === 0 || orientation === 2) {
    rotatedDocumentDimensions.addPage(400, 500);
  } else {
    rotatedDocumentDimensions.addPage(500, 400);
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
        mockPlugin.findMessage<{type: string}>('getAnnotationBrush');
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
        mockPlugin.findMessage<{type: string}>('getAllTextAnnotations');
    chrome.test.assertTrue(getAllTextAnnotationsMessage !== undefined);
    chrome.test.assertEq(
        'getAllTextAnnotations', getAllTextAnnotationsMessage.type);

    chrome.test.succeed();
  },

  async function testInitializeTextNonEmpty() {
    manager.clearAnnotationsForTesting();
    manager.resetTextResolverForTesting();

    // Set the reply to getAllTextAnnotations to return non-empty.
    const testAnnotation1 = getTestAnnotation(0);
    const testAnnotation2 = getTestAnnotation(1);
    testAnnotation2.text = 'Goodbye Moon';
    testAnnotation2.textBoxRect = {
      height: 25,
      locationX: 60,
      locationY: 65,
      width: 50,
    };
    testAnnotation2.textAttributes.color = {r: 100, g: 0, b: 0};
    testAnnotation2.textAttributes.size = 10;
    mockPlugin.clearMessages();
    mockPlugin.setMessageReply('getAllTextAnnotations', {
      annotations: [testAnnotation1, testAnnotation2],
    });

    chrome.test.assertFalse(manager.isTextInitializationComplete());
    await manager.initializeTextAnnotations();
    chrome.test.assertTrue(manager.isTextInitializationComplete());

    // Check that the manager requested all the text annotations.
    const getAllTextAnnotationsMessage =
        mockPlugin.findMessage<{type: string}>('getAllTextAnnotations');
    chrome.test.assertTrue(getAllTextAnnotationsMessage !== undefined);
    chrome.test.assertEq(
        'getAllTextAnnotations', getAllTextAnnotationsMessage.type);

    // Check that the two existing annotations can be activated.
    mockPlugin.clearMessages();
    const whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    const created1 = await manager.initializeTextAnnotation({x: 120, y: 30});
    chrome.test.assertTrue(created1);
    let initEvent = await whenInitEvent;
    const testAnnotation1ScreenCoords = structuredClone(testAnnotation1);
    // Add page offsets. These are the defaults for the test viewport setup
    // of a 400x500 page in a 500x500 window.
    testAnnotation1ScreenCoords.textBoxRect.locationX =
        testAnnotation1.textBoxRect.locationX + 55;
    testAnnotation1ScreenCoords.textBoxRect.locationY =
        testAnnotation1.textBoxRect.locationY + 3;
    assertDeepEquals(testAnnotation1ScreenCoords, initEvent.detail.annotation);
    // Verify that the init event does not contain a reference to the current
    // attributes.
    chrome.test.assertFalse(
        initEvent.detail.annotation.textAttributes ===
        manager.getCurrentTextAttributes());
    verifyEditTextAnnotationMessage(true, testAnnotation1.id);

    // Make the first text box active.
    manager.setTextBoxActive(true);

    // Simulate making a change.
    const whenUpdatedColor = eventToPromise<CustomEvent<TextAttributes>>(
        'attributes-changed', manager);
    const blue = {r: 0, g: 0, b: 100};
    manager.setTextColor(blue);
    const updateEvent = await whenUpdatedColor;
    // Check that the event does not fire with a reference to the current
    // attributes.
    chrome.test.assertFalse(
        updateEvent.detail === manager.getCurrentTextAttributes());
    testAnnotation1ScreenCoords.textAttributes = updateEvent.detail;

    mockPlugin.clearMessages();
    initEvent = await changeActiveAnnotation(
        {x: 120, y: 70}, true, testAnnotation1ScreenCoords);

    // Confirm that the finish annotation message is sent with the correct
    // parameters.
    const finishTextAnnotationMessage =
        mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
            'finishTextAnnotation');
    chrome.test.assertTrue(finishTextAnnotationMessage !== undefined);
    chrome.test.assertEq(
        'finishTextAnnotation', finishTextAnnotationMessage.type);
    const expectedMessageData1: TextAnnotationMessageData = {
      ...testAnnotation1,
      isEdited: true,
      mojoTextInfo: new ArrayBuffer(0),
      newTypefaces: [],
      source: TextAnnotationSource.USER,
    };
    expectedMessageData1.textAttributes.color = blue;
    assertDeepEquals(expectedMessageData1, finishTextAnnotationMessage.data);

    // Confirm annotation 2 is initialized with the correct parameters.
    const testAnnotation2ScreenCoords = structuredClone(testAnnotation2);
    testAnnotation2ScreenCoords.textBoxRect.locationX =
        testAnnotation2.textBoxRect.locationX + 55;
    testAnnotation2ScreenCoords.textBoxRect.locationY =
        testAnnotation2.textBoxRect.locationY + 3;
    assertDeepEquals(testAnnotation2ScreenCoords, initEvent.detail.annotation);
    verifyEditTextAnnotationMessage(true, testAnnotation2.id);

    // Make the second text box active.
    manager.setTextBoxActive(true);

    // Check that initializing a new annotation in a different location sets
    // a different id, and uses the default settings.
    mockPlugin.clearMessages();
    initEvent = await changeActiveAnnotation({x: 200, y: 200}, true);
    chrome.test.assertEq(2, initEvent.detail.annotation.id);
    chrome.test.assertEq('', initEvent.detail.annotation.text);
    assertDeepEquals(
        {r: 0, b: 0, g: 0}, initEvent.detail.annotation.textAttributes.color);
    assertDeepEquals(
        {bold: false, italic: false},
        initEvent.detail.annotation.textAttributes.styles);
    chrome.test.assertEq(12, initEvent.detail.annotation.textAttributes.size);
    verifyEditTextAnnotationMessage(false);

    manager.setTextBoxActive(false);
    mockPlugin.clearMessages();
    chrome.test.succeed();
  },

  async function testInitializeTextSingleLoadedAnnotation() {
    manager.clearAnnotationsForTesting();
    manager.resetTextResolverForTesting();

    // Set the reply to getAllTextAnnotations to return a single loaded
    // annotation with ID 0.
    const testAnnotation = getTestAnnotation(0);
    mockPlugin.clearMessages();
    mockPlugin.setMessageReply('getAllTextAnnotations', {
      annotations: [testAnnotation],
    });

    chrome.test.assertFalse(manager.isTextInitializationComplete());
    await manager.initializeTextAnnotations();
    chrome.test.assertTrue(manager.isTextInitializationComplete());

    // Check that initializing a new annotation gets ID 1 (not 0, to avoid
    // collision).
    mockPlugin.clearMessages();
    const whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    const created = await manager.initializeTextAnnotation({x: 200, y: 200});
    chrome.test.assertTrue(created);
    const initEvent = await whenInitEvent;
    chrome.test.assertEq(1, initEvent.detail.annotation.id);
    verifyEditTextAnnotationMessage(false);

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
    const blue = {r: 0, g: 0, b: 100};
    manager.setTextColor(blue);
    expectedAttributes.color = blue;
    assertTextUpdate(3, expectedAttributes);

    // Set style to bold + italic.
    const boldItalic = {bold: true, italic: true};
    manager.setTextStyles(boldItalic);
    expectedAttributes.styles = boldItalic;
    assertTextUpdate(4, expectedAttributes);

    // Reset for subsequent tests.
    resetFontProperties();

    chrome.test.succeed();
  },

  async function testNoInitializeOutsidePage() {
    let initEvents = 0;
    manager.addEventListener('initialize-text-box', () => {
      initEvents++;
    });

    // x offset is (viewportWidth - documentWidth) / 2 + shadow = 55. A click
    // anywhere to the left of that should not initialize an annotation and
    // should return false.
    const created1 = await Ink2Manager.getInstance().initializeTextAnnotation(
        {x: 40, y: 20});
    chrome.test.assertFalse(created1);
    chrome.test.assertEq(0, initEvents);

    // Similarly, we have 55px of margin on the right side where a click should
    // return false.
    const created2 = await Ink2Manager.getInstance().initializeTextAnnotation(
        {x: 480, y: 400});
    chrome.test.assertFalse(created2);
    chrome.test.assertEq(0, initEvents);

    chrome.test.succeed();
  },

  async function testInitializeTextboxNoLocation() {
    // Use the same values as testSetFontProperties, since this test was
    // originally written with those values accidentally leaking into this test.
    manager.setTextTypeface(TextTypeface.SERIF);
    manager.setTextSize(10);
    manager.setTextAlignment(TextAlignment.CENTER);
    const red = {r: 255, b: 0, g: 0};
    manager.setTextColor(red);
    const boldItalic = {bold: true, italic: true};
    manager.setTextStyles(boldItalic);

    const whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    const created1 = await manager.initializeTextAnnotation();
    chrome.test.assertTrue(created1);
    let initEvent = await whenInitEvent;

    // The full document fits in the window.
    chrome.test.assertEq(
        MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.height);
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
        248 - MIN_TEXTBOX_SIZE_PX / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageIndex);
    chrome.test.assertEq(55, initEvent.detail.pageDimensions.x);
    chrome.test.assertEq(3, initEvent.detail.pageDimensions.y);
    chrome.test.assertEq(390, initEvent.detail.pageDimensions.width);
    chrome.test.assertEq(490, initEvent.detail.pageDimensions.height);

    // Make the text box active.
    manager.setTextBoxActive(true);

    // Zoom to 2.0. Now, the new annotation should be centered on the visible
    // portion of the page.
    viewport.setZoom(2.0);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    initEvent = await changeActiveAnnotation(null, true);

    chrome.test.assertEq(
        MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.height);
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
        253 - MIN_TEXTBOX_SIZE_PX / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageIndex);
    chrome.test.assertEq(10, initEvent.detail.pageDimensions.x);
    chrome.test.assertEq(6, initEvent.detail.pageDimensions.y);
    chrome.test.assertEq(780, initEvent.detail.pageDimensions.width);
    chrome.test.assertEq(980, initEvent.detail.pageDimensions.height);

    // Make the text box active.
    manager.setTextBoxActive(true);

    // Zoom to 0.5. The new box should still be centered on the page, even
    // though it is not centered in the viewport.
    viewport.setZoom(0.5);
    // Initialize without a location. This is what happens when the user creates
    // a textbox by using "Enter" on the plugin, instead of with the mouse.
    initEvent = await changeActiveAnnotation(null, true);

    chrome.test.assertEq(
        MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.height);

    // The default width goes down from 222 to the width of the page, which is
    // 195 at 0.5 zoom ( 400 * 0.5 - 2 * shadow ).
    // The page is centered on the viewport in the x direction, so it runs from
    // 152.5 to 347.5. The center is at (152.5 + 347.5) / 2 = 250. Subtract half
    // the default width to get the left edge.
    chrome.test.assertEq(
        250 - 195 / 2, initEvent.detail.annotation.textBoxRect.locationX);
    // The page starts at the y margin/shadow of 1.5 and runs to 246.5 since it
    // is 245 tall without the shadows. The center is at
    // (1.5 + 246.5) / 2 = 124. Subtract half the default height to get the top
    // edge.
    chrome.test.assertEq(
        124 - MIN_TEXTBOX_SIZE_PX / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(195, initEvent.detail.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initEvent.detail.annotation.pageIndex);
    chrome.test.assertEq(152.5, initEvent.detail.pageDimensions.x);
    chrome.test.assertEq(1.5, initEvent.detail.pageDimensions.y);
    chrome.test.assertEq(195, initEvent.detail.pageDimensions.width);
    chrome.test.assertEq(245, initEvent.detail.pageDimensions.height);

    // Reset zoom and annotation id for next test.
    viewport.setZoom(1.0);
    manager.clearAnnotationsForTesting();
    manager.setTextBoxActive(false);

    // Reset for subsequent tests.
    resetFontProperties();

    chrome.test.succeed();
  },

  async function testInitializeTextboxClampToPage() {
    const whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);

    // Test initializing with the top left corner of the box near the right edge
    // of the page. Instead of initializing at this point, this should
    // initialize within the page boundaries.
    const created1 = await manager.initializeTextAnnotation({x: 425, y: 400});
    chrome.test.assertTrue(created1);
    let initEvent = await whenInitEvent;

    chrome.test.assertEq(
        MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.height);
    // Offset the box from the page edge by the width of the box. The box gets a
    // width of 2x the minimum allowed by Blink.
    chrome.test.assertEq(
        445 - 2 * MIN_TEXTBOX_SIZE_PX,
        initEvent.detail.annotation.textBoxRect.locationX);
    // y doesn't need adjusted in this case, since we're far enough from the
    // bottom boundary of the page. y is always offset by half the text height.
    chrome.test.assertEq(
        400 - manager.getCurrentTextAttributes().size / 2,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        2 * MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.width);

    // Make the text box active.
    manager.setTextBoxActive(true);

    // Now test initializing very close to the bottom of the page. This should
    // instead initialize far enough from bottom to fit the box.
    initEvent = await changeActiveAnnotation({x: 200, y: 490}, true);

    chrome.test.assertEq(
        MIN_TEXTBOX_SIZE_PX, initEvent.detail.annotation.textBoxRect.height);
    chrome.test.assertEq(
        200, initEvent.detail.annotation.textBoxRect.locationX);
    chrome.test.assertEq(
        493 - MIN_TEXTBOX_SIZE_PX,
        initEvent.detail.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initEvent.detail.annotation.textBoxRect.width);

    // Reset annotation id for next test.
    manager.clearAnnotationsForTesting();
    manager.setTextBoxActive(false);

    chrome.test.succeed();
  },

  async function testInitializeTextBox() {
    // Add listeners for the expected events that fire in response to an
    // initializeTextAnnotation call. We only need to collect attributes-changed
    // events here, as initialize-text-box is handled by the helper.
    let attributesChangedEvents: Array<CustomEvent<TextAttributes>> = [];
    manager.addEventListener('attributes-changed', e => {
      attributesChangedEvents.push(e as CustomEvent<TextAttributes>);
    });

    const attributes = manager.getCurrentTextAttributes();
    async function verifyTextboxInit(
        x: number, y: number, rotation: number, id: number) {
      const initEvent = await changeActiveAnnotation({x, y}, id > 0);

      const initData = initEvent.detail;
      chrome.test.assertEq('', initData.annotation.text);
      assertDeepEquals(attributes, initData.annotation.textAttributes);
      chrome.test.assertEq(
          MIN_TEXTBOX_SIZE_PX, initData.annotation.textBoxRect.height);
      chrome.test.assertEq(x, initData.annotation.textBoxRect.locationX);
      chrome.test.assertEq(
          y - manager.getCurrentTextAttributes().size / 2,
          initData.annotation.textBoxRect.locationY);
      chrome.test.assertEq(
          DEFAULT_TEXTBOX_WIDTH, initData.annotation.textBoxRect.width);
      chrome.test.assertEq(0, initData.annotation.pageIndex);
      chrome.test.assertEq(id, initData.annotation.id);
      chrome.test.assertEq(rotation, initData.annotation.textOrientation);
      // Placeholder viewport has a 400x500 page and 500x500 window.
      // The y offset is always 3px, because the page is always positioned
      // 3px from the top. When the page is oriented vertically, it is centered
      // in the viewport with an additional 5px margin in x, creating pageX =
      // (500 - 400)/2 + 5 = 55px offset. When the page is oriented
      // horizontally, it is as wide as the viewport, so it uses the minimum
      // 5px margin for pageX.
      chrome.test.assertEq(
          rotation % 2 === 0 ? 55 : 5, initData.pageDimensions.x);
      chrome.test.assertEq(3, initData.pageDimensions.y);

      chrome.test.assertEq(1, attributesChangedEvents.length);
      assertDeepEquals(attributes, attributesChangedEvents[0]!.detail);
      attributesChangedEvents = [];

      // Since this is a new annotation, it shouldn't have sent a message to the
      // plugin.
      verifyEditTextAnnotationMessage(false);
    }

    // Test initialization in different positions and different viewport
    // rotations. id should increment with each new textbox.
    rotateViewport(/* clockwiseRotations= */ 3);
    await verifyTextboxInit(/* x= */ 15, /* y= */ 10, /* rotations= */ 1,
                            /* id= */ 0);
    rotateViewport(/* clockwiseRotations= */ 2);
    await verifyTextboxInit(/* x= */ 60, /* y= */ 60, /* rotations= */ 2,
                            /* id= */ 1);
    rotateViewport(/* clockwiseRotations= */ 1);
    await verifyTextboxInit(/* x= */ 80, /* y = */ 20, /* rotations= */ 3,
                            /* id= */ 2);
    rotateViewport(/* clockwiseRotations= */ 0);
    await verifyTextboxInit(/* x= */ 200, /* y= */ 23, /* rotations= */ 0,
                            /* id= */ 3);

    // Clean up active textbox.
    manager.setTextBoxActive(false);
    chrome.test.succeed();
  },

  function testCommitTextAnnotation() {
    function testCommitAnnotation(
        annotationScreenCoords: TextAnnotation,
        annotationPageCoords: TextAnnotationMessageData) {
      // Committing with edited = true should fire a modified event.
      // Use structuredClone since the manager edits the object in place,
      // and we want to reuse this below.
      annotationPageCoords.isEdited = true;
      const editedAnnot = structuredClone(annotationScreenCoords);
      manager.commitTextAnnotation(editedAnnot, true, []);
      verifyFinishTextAnnotationMessage(annotationPageCoords);
      mockPlugin.clearMessages();

      // Committing with edited = false should fire an unmodified event.
      annotationPageCoords.isEdited = false;
      const uneditedAnnot = structuredClone(annotationScreenCoords);
      manager.commitTextAnnotation(uneditedAnnot, false, []);
      verifyFinishTextAnnotationMessage(annotationPageCoords);
      mockPlugin.clearMessages();
    }

    // Test committing annotations at different rotations to ensure the
    // conversion back to page coordinates works correctly. Note that the
    // page screen rectangle will be 390x490 since there are 10px of page
    // shadow.

    // 90 degrees CCW
    rotateViewport(/* clockwiseRotations= */ 3);
    let annotationScreenCoords: TextAnnotation = getTestAnnotation(3);
    let annotationPageCoords: TextAnnotationMessageData =
        getTestAnnotationMessageData(3);
    annotationPageCoords.textBoxRect = {
      height: 50,
      width: 35,
      locationX: 333,
      locationY: 55,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true, []);
    mockPlugin.clearMessages();

    // 180 degrees
    rotateViewport(/* clockwiseRotations= */ 2);
    annotationScreenCoords = getTestAnnotation(2);
    annotationPageCoords = getTestAnnotationMessageData(2);
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect = {
      height: 35,
      width: 50,
      locationX: 335,
      locationY: 433,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true, []);
    mockPlugin.clearMessages();

    // 90 degrees CW
    rotateViewport(/* clockwiseRotations= */ 1);
    annotationScreenCoords = getTestAnnotation(1);
    annotationPageCoords = getTestAnnotationMessageData(1);
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect = {
      height: 50,
      width: 35,
      locationX: 22,
      locationY: 385,
    };
    testCommitAnnotation(annotationScreenCoords, annotationPageCoords);
    // Delete to clear state.
    annotationScreenCoords.text = '';
    manager.commitTextAnnotation(annotationScreenCoords, true, []);
    mockPlugin.clearMessages();

    // Normal orientation (0 degrees).
    rotateViewport(/* clockwiseRotations= */ 0);
    annotationScreenCoords = getTestAnnotation(0);
    annotationPageCoords = getTestAnnotationMessageData(0);
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
    const eventsDispatched:
        Array<{name: string, detail: TextBoxInit | TextAttributes}> = [];
    ['initialize-text-box', 'attributes-changed'].forEach(eventName => {
      manager.addEventListener(eventName, e => {
        eventsDispatched.push(
            {name: eventName, detail: (e as CustomEvent).detail});
      });
    });

    const whenUpdateEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    const created = await Ink2Manager.getInstance().initializeTextAnnotation(
        {x: 80, y: 40});
    chrome.test.assertTrue(created);
    await whenUpdateEvent;
    chrome.test.assertEq(2, eventsDispatched.length);
    chrome.test.assertEq('initialize-text-box', eventsDispatched[0]!.name);
    const initData = eventsDispatched[0]!.detail as TextBoxInit;
    const testAnnotation = getTestAnnotation(0);
    assertDeepEquals(testAnnotation, initData.annotation);
    // Still using the 400x500 page from the previous test.
    chrome.test.assertEq(55, initData.pageDimensions.x);
    chrome.test.assertEq(3, initData.pageDimensions.y);
    chrome.test.assertEq(390, initData.pageDimensions.width);
    chrome.test.assertEq(490, initData.pageDimensions.height);
    chrome.test.assertEq('attributes-changed', eventsDispatched[1]!.name);
    assertDeepEquals(
        testAnnotation.textAttributes, eventsDispatched[1]!.detail);

    // Since this is an existing annotation, it should send an edit message to
    // the plugin.
    verifyEditTextAnnotationMessage(true);

    chrome.test.succeed();
  },

  async function testViewport() {
    const initialParams = manager.getViewportParams();
    chrome.test.assertEq(1.0, initialParams.zoom);
    // pageMarginY * zoom = 3 * 1
    chrome.test.assertEq(3, initialParams.pageDimensions.y);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (500 - 400 * 1)/2 + 5 * 1
    chrome.test.assertEq(55, initialParams.pageDimensions.x);
    // 10px of width are taken up by PAGE_SHADOW.
    chrome.test.assertEq(390, initialParams.pageDimensions.width);
    // 20px of height are also taken up by PAGE_SHADOW.
    chrome.test.assertEq(490, initialParams.pageDimensions.height);
    chrome.test.assertEq(0, initialParams.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 60, 25 in screen coordinates. Make
    // sure clicking there creates the box, and clicking just outside of this
    // does not.
    mockPlugin.clearMessages();
    const created = await manager.initializeTextAnnotation({x: 60, y: 25});
    chrome.test.assertTrue(created);
    verifyEditTextAnnotationMessage(true);
    manager.setTextBoxActive(true);

    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 59, y: 24}, true);
    verifyEditTextAnnotationMessage(false);

    // Zoom out should fire an event.
    let whenViewportChanged = eventToPromise<CustomEvent<ViewportParams>>(
        'viewport-changed', manager);
    viewport.setZoom(0.5);
    let changedEvent = await whenViewportChanged;
    chrome.test.assertEq(0.5, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * .5
    chrome.test.assertEq(1.5, changedEvent.detail.pageDimensions.y);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (500 - 400 * .5)/2 + 5 * .5
    chrome.test.assertEq(152.5, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(195, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(245, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 155, 12.5 in screen coordinates.
    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 155, y: 13}, true);
    verifyEditTextAnnotationMessage(true);

    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 154, y: 12}, true);
    verifyEditTextAnnotationMessage(false);

    // Zoom in should fire an event.
    whenViewportChanged = eventToPromise<CustomEvent<ViewportParams>>(
        'viewport-changed', manager);
    viewport.setZoom(2.0);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * 2
    chrome.test.assertEq(6, changedEvent.detail.pageDimensions.y);
    // docWidth * zoom > windowWidth, so this is now pageMarginX * zoom = 5 * 2
    chrome.test.assertEq(10, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(780, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(980, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 25, 50 in screen coordinates.
    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 25, y: 50}, true);
    verifyEditTextAnnotationMessage(true);

    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 24, y: 49}, true);
    verifyEditTextAnnotationMessage(false);

    // Translation.
    whenViewportChanged = eventToPromise<CustomEvent<ViewportParams>>(
        'viewport-changed', manager);
    viewport.goToPageAndXy(0, 20, 20);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-34, changedEvent.detail.pageDimensions.y);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-30, changedEvent.detail.pageDimensions.x);
    chrome.test.assertEq(780, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(980, changedEvent.detail.pageDimensions.height);
    chrome.test.assertEq(0, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at -15, 10 in screen coordinates.
    // It has width 100 and height 70 so (0, 81) should be just outside the box
    // and (0, 80) just inside.
    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 0, y: 80}, true);
    verifyEditTextAnnotationMessage(true);

    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 0, y: 81}, true);
    verifyEditTextAnnotationMessage(false);

    // Rotation
    whenViewportChanged = eventToPromise<CustomEvent<ViewportParams>>(
        'viewport-changed', manager);
    rotateViewport(/* clockwiseRotations= */ 3);  // 90 degree CCW rotation.
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    chrome.test.assertEq(-34, changedEvent.detail.pageDimensions.y);
    chrome.test.assertEq(-30, changedEvent.detail.pageDimensions.x);
    // Width and height are switched.
    chrome.test.assertEq(980, changedEvent.detail.pageDimensions.width);
    chrome.test.assertEq(780, changedEvent.detail.pageDimensions.height);
    // Rotations now non-zero.
    chrome.test.assertEq(3, changedEvent.detail.clockwiseRotations);

    // In this new layout, the existing 50x35 annotation at page coordinate
    // 5, 22 has its top left corner at 14, 636 in screen coordinates. This
    // is outside the viewport, which is only 500px tall. Scroll down by 200, or
    // 100 more in page coordinates, to put the box at 14, 436 so it can be
    // activated.
    viewport.goToPageAndXy(0, 20, 120);
    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 84, y: 436}, true);
    verifyEditTextAnnotationMessage(true);

    mockPlugin.clearMessages();
    await changeActiveAnnotation({x: 85, y: 436}, true);
    verifyEditTextAnnotationMessage(false);

    // Reset state for next test.
    manager.setTextBoxActive(false);
    manager.clearAnnotationsForTesting();
    viewport.setZoom(1.0);
    rotateViewport(/* clockwiseRotations= */ 0);  // 0 rotation.
    viewport.goToPageAndXy(0, 0, 0);

    chrome.test.succeed();
  },

  async function testTextboxFocused() {
    // Simulate creating a textbox at (255, 250) (near center of the viewport).
    const created = await manager.initializeTextAnnotation({x: 255, y: 250});
    chrome.test.assertTrue(created);

    // Zoom by 2x. This would cause the textbox to be out of the view, at
    // location 510, 500.
    viewport.setZoom(2.0);

    mockPlugin.clearMessages();

    // Now simulate focus moving to the textbox (e.g., because the user tabbed
    // there). Make sure the manager sends a message to the plugin to scroll
    // the viewport to the textbox.
    manager.textBoxFocused({
      locationX: 510,
      locationY: 500,
      height: 50,
      width: 50,
    });

    let syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // The content is 800x1000 (2x page size), and the viewport is 500x500. The
    // scrollbar is 5px wide.
    // Max scroll is contentSize - viewportSize + scrollbarWidth.
    // The max scroll is therefore 800 - 500 + 5 = 305 horizontally, so in the x
    // direction scroll is clamped at 305, which is less than the desired scroll
    // of 510 - .1 * viewportWidth = 460.
    // Vertically, the desired y position is 500 - .1 * viewportHeight = 450,
    // which is within the max scroll of
    // contentHeight - viewportHeight + scrollbarWidth = 505.
    chrome.test.assertEq(305, syncScrollMessage.x);
    chrome.test.assertEq(450, syncScrollMessage.y);

    // Focusing a textbox that is already in the view shouldn't scroll the
    // viewport.
    mockPlugin.clearMessages();
    manager.textBoxFocused({
      locationX: 45,
      locationY: 10,
      height: 50,
      width: 50,
    });
    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertEq(undefined, syncScrollMessage);

    // Focus a textbox that is out of bounds the other direction.
    manager.textBoxFocused({
      locationX: -20,
      locationY: -10,
      height: 50,
      width: 50,
    });
    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    // Scroll x to maxScrollWidth - 20 - .1 * viewportWidth = 305 - 20 - 50 =
    // 235.
    // Scroll y to currentScrollY - 10 - .1 * viewportHeight =
    // 450 - 10 - 50 = 390.
    chrome.test.assertEq(235, syncScrollMessage.x);
    chrome.test.assertEq(390, syncScrollMessage.y);

    // Reset zoom and annotation id for next test.
    viewport.setZoom(1.0);
    manager.clearAnnotationsForTesting();

    chrome.test.succeed();
  },

  function testFontCaching() {
    assertDeepEquals([], manager.getKnownFontIds());

    manager.addKnownFontId(1);
    assertDeepEquals([1], manager.getKnownFontIds());
    manager.addKnownFontId(2);
    assertDeepEquals([1, 2], manager.getKnownFontIds());

    // verify that getKnownFontIds() returns a copy.
    const ids = manager.getKnownFontIds();
    ids.push(3);
    assertDeepEquals([1, 2], manager.getKnownFontIds());

    chrome.test.succeed();
  },

  function testUndoRedoStack() {
    manager.resetStackForTesting();

    let lastState: UndoRedoStateChangedDetail|null = null;
    Ink2Manager.getInstance().addEventListener(
        'undo-redo-state-changed', (e: Event) => {
          lastState = (e as CustomEvent<UndoRedoStateChangedDetail>).detail;
        });

    // Helper to assert stack state
    function assertStackState(
        canUndo: boolean, canRedo: boolean, hasUnsavedEdits: boolean) {
      assert(lastState);
      chrome.test.assertEq(canUndo, lastState.canUndo);
      chrome.test.assertEq(canRedo, lastState.canRedo);
      chrome.test.assertEq(hasUnsavedEdits, lastState.hasUnsavedEdits);
    }

    // 1. Finish ink stroke adds to the stack
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(
            PluginControllerEventType.FINISH_INK_STROKE, {detail: true}));
    assertStackState(true, false, true);

    // 2. Commit text annotation (edited) - adds to the stack
    const testAnnotation = getTestAnnotation(0);
    manager.commitTextAnnotation(testAnnotation, true, []);
    assertStackState(true, false, true);

    // 3. Commit a text annotation with no edits. This should not impact
    //    the undo/redo stack or fire an event.
    lastState = null;
    const testAnnotation2 = getTestAnnotation(1);
    manager.commitTextAnnotation(testAnnotation2, false, []);
    // Since it wasn't edited, no 'undo-redo-state-changed' event should have
    // fired.
    chrome.test.assertTrue(lastState === null);

    // 4. Finish ink stroke adds to the stack
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(
            PluginControllerEventType.FINISH_INK_STROKE, {detail: true}));
    // Stack now has: [Ink, Text, Ink]
    assertStackState(true, false, true);

    // 5. Undo
    manager.undo();
    assertStackState(true, true, true);  // Pointer moved back, can redo now

    // 6. Undo again
    manager.undo();
    assertStackState(true, true, true);

    // 7. Undo again (back to start)
    manager.undo();
    // No unsaved edits, and can't undo any more.
    assertStackState(false, true, false);

    // 8. Redo
    manager.redo();
    assertStackState(true, true, true);  // Can undo and redo

    // 9. Save
    manager.initiateSave();
    manager.saved();
    assertStackState(true, true, false);  // No unsaved edits anymore

    // 10. Push new action after save
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(
            PluginControllerEventType.FINISH_INK_STROKE, {detail: true}));
    // Can't redo any more, because the previous action/annotation has been
    // removed. Stack is dirty again due to the new action.
    assertStackState(true, false, true);

    // 11. Undo to the last save
    manager.undo();                       // Undo the new action
    assertStackState(true, true, false);  // Back to saved state, not dirty.

    // 12. Undo past the last save.
    manager.undo();
    // Dirty again (saved action was removed).
    assertStackState(false, true, true);

    // 13. Redo back to save
    manager.redo();
    assertStackState(true, true, false);  // Clean again

    // Reset state for next test.
    manager.setTextBoxActive(false);
    manager.resetStackForTesting();
    manager.clearAnnotationsForTesting();
    mockPlugin.clearMessages();

    chrome.test.succeed();
  },

  async function testUndoRedoTextAnnotationContent() {
    // Helper to check if annotation exists in manager
    function assertAnnotationExists(
        id: number, exists: boolean, expectedText?: string) {
      const pageAnnots = manager.getAnnotationsForTesting().get(0);
      if (exists) {
        chrome.test.assertTrue(pageAnnots !== undefined);
        const annot = pageAnnots.get(id);
        chrome.test.assertTrue(annot !== undefined);
        if (expectedText !== undefined) {
          chrome.test.assertEq(expectedText, annot.text);
        }
      } else if (pageAnnots) {
        chrome.test.assertTrue(pageAnnots.get(id) === undefined);
      }
    }

    // --- 1. TEST CREATION ---
    // Initialize new annotation (id 0)
    let whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    const created1 = await manager.initializeTextAnnotation({x: 100, y: 100});
    chrome.test.assertTrue(created1);
    let initEvent = await whenInitEvent;
    const annot0 = initEvent.detail.annotation;
    chrome.test.assertEq(0, annot0.id);
    annot0.text = 'Hello';

    // Set up expectation with values shared by all checks.
    const expectedMessage = getTestAnnotationMessageData(0);
    expectedMessage.textBoxRect.width = 222;
    expectedMessage.textBoxRect.height = 24;
    expectedMessage.textBoxRect.locationX = 45;
    expectedMessage.textBoxRect.locationY = 91;
    expectedMessage.isEdited = true;

    // Commit creation
    manager.commitTextAnnotation(annot0, true, []);
    // New message is from the user.
    expectedMessage.text = 'Hello';
    expectedMessage.source = TextAnnotationSource.USER;
    verifyFinishTextAnnotationMessage(expectedMessage);
    assertAnnotationExists(0, true, 'Hello');
    mockPlugin.clearMessages();

    // Undo creation -> should delete it
    manager.undo();
    assertAnnotationExists(0, false);
    // Deletion sends a message to the plugin with empty text
    expectedMessage.text = '';
    expectedMessage.source = TextAnnotationSource.UNDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Redo creation -> should restore it
    manager.redo();
    assertAnnotationExists(0, true, 'Hello');
    expectedMessage.text = 'Hello';
    expectedMessage.source = TextAnnotationSource.REDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // --- 2. TEST MODIFICATION ---
    // Initialize an existing annotation (id 0) for edit
    whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    // Click in same place.
    const created2 = await manager.initializeTextAnnotation({x: 100, y: 100});
    chrome.test.assertTrue(created2);
    initEvent = await whenInitEvent;
    const annot0Edit = initEvent.detail.annotation;
    chrome.test.assertEq(0, annot0Edit.id);
    annot0Edit.text = 'World';

    // Commit modification, which is from the user.
    manager.commitTextAnnotation(annot0Edit, true, []);
    expectedMessage.text = 'World';
    expectedMessage.source = TextAnnotationSource.USER;
    verifyFinishTextAnnotationMessage(expectedMessage);
    assertAnnotationExists(0, true, 'World');
    mockPlugin.clearMessages();

    // Undo modification -> should restore to 'Hello'
    manager.undo();
    assertAnnotationExists(0, true, 'Hello');
    expectedMessage.text = 'Hello';
    expectedMessage.source = TextAnnotationSource.UNDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Redo modification -> should change back to 'World'
    manager.redo();
    assertAnnotationExists(0, true, 'World');
    expectedMessage.text = 'World';
    expectedMessage.source = TextAnnotationSource.REDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // --- 3. TEST DELETION ---
    // Initialize existing annotation (id 0) for edit
    whenInitEvent = eventToPromise<CustomEvent<TextBoxInit>>(
        'initialize-text-box', manager);
    const created3 = await manager.initializeTextAnnotation({x: 100, y: 100});
    chrome.test.assertTrue(created3);
    initEvent = await whenInitEvent;
    const annot0Delete = initEvent.detail.annotation;
    // Empty text deletes the annotation, and matches what ink-text-box does
    // when "Delete" is pressed.
    annot0Delete.text = '';
    manager.commitTextAnnotation(annot0Delete, true, []);
    expectedMessage.text = '';
    expectedMessage.source = TextAnnotationSource.USER;
    verifyFinishTextAnnotationMessage(expectedMessage);
    assertAnnotationExists(0, false);
    mockPlugin.clearMessages();

    // Undo deletion -> should restore to 'World'
    manager.undo();
    assertAnnotationExists(0, true, 'World');
    expectedMessage.text = 'World';
    expectedMessage.source = TextAnnotationSource.UNDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Redo deletion -> should delete again
    manager.redo();
    assertAnnotationExists(0, false);
    expectedMessage.text = '';
    expectedMessage.source = TextAnnotationSource.REDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Reset state for next test.
    manager.setTextBoxActive(false);
    manager.resetStackForTesting();
    manager.clearAnnotationsForTesting();

    chrome.test.succeed();
  },

  async function testUndoRedoWithZoomChange() {
    // Draw a text box (id 0) at zoom 1.0
    const initEvent = await changeActiveAnnotation({x: 100, y: 100}, false);
    const annot = initEvent.detail.annotation;
    annot.text = 'Zoom';

    // Set up expectation with values shared by all checks.
    const expectedMessage = getTestAnnotationMessageData(0);
    expectedMessage.textBoxRect.width = 222;
    expectedMessage.textBoxRect.height = 24;
    expectedMessage.textBoxRect.locationX = 45;
    expectedMessage.textBoxRect.locationY = 91;
    expectedMessage.isEdited = true;

    // Commit creation
    manager.commitTextAnnotation(annot, true, []);
    expectedMessage.text = 'Zoom';
    expectedMessage.source = TextAnnotationSource.USER;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Undo
    manager.undo();
    expectedMessage.text = '';
    expectedMessage.source = TextAnnotationSource.UNDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Zoom to 2.0 and redo.
    viewport.setZoom(2.0);
    manager.redo();
    expectedMessage.text = 'Zoom';
    expectedMessage.source = TextAnnotationSource.REDO;
    verifyFinishTextAnnotationMessage(expectedMessage);
    mockPlugin.clearMessages();

    // Reset state for next test.
    manager.setTextBoxActive(false);
    manager.resetStackForTesting();
    manager.clearAnnotationsForTesting();

    chrome.test.succeed();
  },
]);
