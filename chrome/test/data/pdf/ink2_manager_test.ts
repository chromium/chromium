// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, TextAnnotation, TextAttributes, TextBoxInit} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, DEFAULT_TEXTBOX_HEIGHT, DEFAULT_TEXTBOX_WIDTH, Ink2Manager, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertDeepEquals, setGetAnnotationBrushReply, setupTestViewportAndMockPluginForInk} from './test_util.js';

const {viewport, mockPlugin} = setupTestViewportAndMockPluginForInk();
const manager = Ink2Manager.getInstance();

function getTestAnnotation(): TextAnnotation {
  return {
    textAttributes: {
      typeface: 'Roboto',
      size: 12,
      color: {r: 0, g: 100, b: 0},
      alignment: TextAlignment.LEFT,
      styles: {
        bold: false,
        italic: false,
        underline: false,
        strikethrough: true,
      },
    },
    text: 'Hello World',
    id: 0,
    pageNumber: 0,
    textBoxRect: {
      height: 50,
      locationX: 15,
      locationY: 25,
      width: 50,
    },
  };
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

  async function testGetTextAnnotationFontNames() {
    // Checks that requesting the fonts for the first time retrieves the
    // fonts from the plugin and fires a attributes-changed event with the font
    // set to the first font returned.
    const whenChanged = eventToPromise('attributes-changed', manager);
    const fontNames = await manager.getTextAnnotationFontNames();

    // For now, these are hardcoded in controller.ts.
    const expectedFontNames = ['Roboto', 'Serif', 'Sans', 'Monospace'];
    chrome.test.assertEq(fontNames.length, expectedFontNames.length);
    for (let i = 0; i < expectedFontNames.length; i++) {
      chrome.test.assertEq(fontNames[i], expectedFontNames[i]);
    }

    // Check that the manager requested the fonts.
    const getTextAnnotFontNamesMessage =
        mockPlugin.findMessage('getTextAnnotFontNames');
    chrome.test.assertTrue(getTextAnnotFontNamesMessage !== undefined);
    chrome.test.assertEq(
        'getTextAnnotFontNames', getTextAnnotFontNamesMessage.type);

    // Check that an event was fired.
    const changedEvent = await whenChanged;
    chrome.test.assertEq('Roboto', changedEvent.detail.typeface);
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
    manager.setTextTypeface('Serif');
    const expectedAttributes = {
      typeface: 'Serif',
      size: 12,
      color: {r: 0, g: 0, b: 0},
      alignment: TextAlignment.LEFT,
      styles: {
        bold: false,
        italic: false,
        underline: false,
        strikethrough: false,
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
    const boldItalic =
        {bold: true, italic: true, underline: false, strikethrough: false};
    manager.setTextStyles(boldItalic);
    expectedAttributes.styles = boldItalic;
    assertTextUpdate(4, expectedAttributes);

    chrome.test.succeed();
  },

  async function testInitializeTextBox() {
    // Add listeners for the expected events that fire in response to an
    // initializeTextAnnotation call.
    const eventsDispatched: Array<{name: string, detail: any}> = [];
    ['initialize-text-box', 'attributes-changed'].forEach(eventName => {
      manager.addEventListener(eventName, e => {
        eventsDispatched.push(
            {name: eventName, detail: (e as CustomEvent).detail});
      });
    });

    const attributes = manager.getCurrentTextAttributes();
    const whenUpdateEvent = eventToPromise('initialize-text-box', manager);
    Ink2Manager.getInstance().initializeTextAnnotation({x: 20, y: 23});
    await whenUpdateEvent;
    chrome.test.assertEq(2, eventsDispatched.length);
    chrome.test.assertEq('initialize-text-box', eventsDispatched[0]!.name);
    const initData = eventsDispatched[0]!.detail as TextBoxInit;
    chrome.test.assertEq('', initData.annotation.text);
    assertDeepEquals(attributes, initData.annotation.textAttributes);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_HEIGHT, initData.annotation.textBoxRect.height);
    chrome.test.assertEq(20, initData.annotation.textBoxRect.locationX);
    chrome.test.assertEq(23, initData.annotation.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, initData.annotation.textBoxRect.width);
    chrome.test.assertEq(0, initData.annotation.pageNumber);
    chrome.test.assertEq(0, initData.annotation.id);
    // Placeholder viewport has a 90x90 page and 100x100 window. This creates
    // pageX and pageY offsets of 10px = (100 - 90)/2 + 5px and 3px
    // respectively.
    chrome.test.assertEq(10, initData.pageCoordinates.x);
    chrome.test.assertEq(3, initData.pageCoordinates.y);
    chrome.test.assertEq('attributes-changed', eventsDispatched[1]!.name);
    assertDeepEquals(attributes, eventsDispatched[1]!.detail);

    // Since this is a new annotation, it shouldn't have sent a message to the
    // plugin.
    const startTextAnnotationMessage =
        mockPlugin.findMessage('startTextAnnotation');
    chrome.test.assertEq(undefined, startTextAnnotationMessage);
    chrome.test.succeed();
  },

  function testCommitTextAnnotation() {
    manager.commitTextAnnotation(getTestAnnotation());
    const finishTextAnnotationMessage =
        mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertTrue(finishTextAnnotationMessage !== undefined);
    chrome.test.assertEq(
        'finishTextAnnotation', finishTextAnnotationMessage.type);
    const annotationPageCoords = getTestAnnotation();
    // Adjust by the x and y offsets to get to page coordinates.
    annotationPageCoords.textBoxRect.locationX = 5;
    annotationPageCoords.textBoxRect.locationY = 22;
    assertDeepEquals(annotationPageCoords, finishTextAnnotationMessage.data);
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
    const testAnnotation = getTestAnnotation();
    assertDeepEquals(testAnnotation, initData.annotation);
    chrome.test.assertEq(10, initData.pageCoordinates.x);
    chrome.test.assertEq(3, initData.pageCoordinates.y);
    chrome.test.assertEq('attributes-changed', eventsDispatched[1]!.name);
    assertDeepEquals(
        testAnnotation.textAttributes, eventsDispatched[1]!.detail);

    // Since this is an existing annotation, it should send a start message to
    // the plugin.
    const startTextAnnotationMessage =
        mockPlugin.findMessage('startTextAnnotation');
    chrome.test.assertTrue(startTextAnnotationMessage !== undefined);
    chrome.test.assertEq(
        'startTextAnnotation', startTextAnnotationMessage.type);
    chrome.test.assertEq(0, startTextAnnotationMessage.data);
    chrome.test.succeed();
  },

  async function testViewport() {
    const initialParams = manager.getViewportParams();
    chrome.test.assertEq(1.0, initialParams.zoom);
    // pageMarginY * zoom = 3 * 1
    chrome.test.assertEq(3, initialParams.pageY);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (100 - 90 * 1)/2 + 5 * 1
    chrome.test.assertEq(10, initialParams.pageX);

    // Zoom out should fire an event.
    let whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(0.5);
    let changedEvent = await whenViewportChanged;
    chrome.test.assertEq(0.5, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * .5
    chrome.test.assertEq(1.5, changedEvent.detail.pageY);
    // (windowWidth - docWidth * zoom)/2 + pageMarginX * zoom =
    // (100 - 90 * .5)/2 + 5 * .5
    chrome.test.assertEq(30, changedEvent.detail.pageX);

    // Zoom in should fire an event.
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.setZoom(2.0);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // pageMarginY * zoom = 3 * 2
    chrome.test.assertEq(6, changedEvent.detail.pageY);
    // docWidth * zoom > windowWidth, so this is now pageMarginX * zoom = 5 * 2
    chrome.test.assertEq(10, changedEvent.detail.pageX);

    // Translation.
    whenViewportChanged = eventToPromise('viewport-changed', manager);
    viewport.goToPageAndXy(0, 20, 20);
    changedEvent = await whenViewportChanged;
    chrome.test.assertEq(2, changedEvent.detail.zoom);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-34, changedEvent.detail.pageY);
    // Shifts by -20 * zoom = -40 from previous position.
    chrome.test.assertEq(-30, changedEvent.detail.pageX);
    chrome.test.succeed();
  },
]);
