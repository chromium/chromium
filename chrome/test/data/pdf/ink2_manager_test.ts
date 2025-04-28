// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, TextAttributes} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, Ink2Manager, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertDeepEquals, setGetAnnotationBrushReply, setupTestViewportAndMockPluginForInk} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

const {viewport, mockPlugin} = setupTestViewportAndMockPluginForInk();
const manager = Ink2Manager.getInstance();

/**
 * Tests that the current annotation text matches `expectedAttributes`. Clears
 * all messages from `mockPlugin` after, otherwise subsequent calls would
 * continue to find and use the same message.
 * @param mockPlugin The mock plugin receiving messages.
 * @param expectedAttributes The expected text attributes that the current
 * annotation text should match.
 */
export function assertTextAttributes(
    mockPlugin: MockPdfPluginElement, expectedAttributes: TextAttributes) {
  const setAnnotationFontMessage =
      mockPlugin.findMessage('setTextAnnotationFont');
  chrome.test.assertTrue(setAnnotationFontMessage !== undefined);
  chrome.test.assertEq('setTextAnnotationFont', setAnnotationFontMessage.type);
  chrome.test.assertEq(
      expectedAttributes.typeface, setAnnotationFontMessage.data.typeface);
  chrome.test.assertEq(
      expectedAttributes.size, setAnnotationFontMessage.data.fontSize);
  chrome.test.assertEq(
      expectedAttributes.alignment, setAnnotationFontMessage.data.alignment);
  assertDeepEquals(
      expectedAttributes.color, setAnnotationFontMessage.data.color);
  assertDeepEquals(
      expectedAttributes.styles, setAnnotationFontMessage.data.style);

  mockPlugin.clearMessages();
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
    // Manager should also have sent the initial text settings to the plugin.
    const expectedAttributes = {
      typeface: 'Roboto',
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
    assertTextAttributes(mockPlugin, expectedAttributes);
    chrome.test.succeed();
  },

  function testSetTextProperties() {
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
    assertTextAttributes(mockPlugin, expectedAttributes);
    assertTextUpdate(0, expectedAttributes);

    // Set size to 10.
    manager.setTextSize(10);
    expectedAttributes.size = 10;
    assertTextAttributes(mockPlugin, expectedAttributes);
    assertTextUpdate(1, expectedAttributes);

    // Set alignment to CENTER.
    manager.setTextAlignment(TextAlignment.CENTER);
    expectedAttributes.alignment = TextAlignment.CENTER;
    assertTextAttributes(mockPlugin, expectedAttributes);
    assertTextUpdate(2, expectedAttributes);

    // Set color to blue.
    const blue = {r: 0, b: 100, g: 0};
    manager.setTextColor(blue);
    expectedAttributes.color = blue;
    assertTextAttributes(mockPlugin, expectedAttributes);
    assertTextUpdate(3, expectedAttributes);

    // Set style to bold + italic.
    const boldItalic =
        {bold: true, italic: true, underline: false, strikethrough: false};
    manager.setTextStyles(boldItalic);
    expectedAttributes.styles = boldItalic;
    assertTextAttributes(mockPlugin, expectedAttributes);
    assertTextUpdate(4, expectedAttributes);

    chrome.test.succeed();
  },

  async function testDispatchUpdateTextBox() {
    const whenUpdateEvent = eventToPromise('update-text-box', manager);
    mockPlugin.dispatchEvent(new MessageEvent('message', {
      data: {
        type: 'updateTextAnnotTextBoxRect',
        height: 50,
        locationX: 150,
        locationY: 250,
        width: 200,
      },
      origin: '*',
    }));
    const event = await whenUpdateEvent;
    chrome.test.assertEq(50, event.detail.height);
    chrome.test.assertEq(150, event.detail.locationX);
    chrome.test.assertEq(250, event.detail.locationY);
    chrome.test.assertEq(200, event.detail.width);
    chrome.test.succeed();
  },

  function testSetTextBoxRect() {
    const newRect = {
      height: 100,
      locationX: 200,
      locationY: 300,
      width: 400,
    };
    manager.setTextBoxRect(newRect);
    const setTextAnnotTextBoxRectMessage =
        mockPlugin.findMessage('setTextAnnotTextBoxRect');
    chrome.test.assertTrue(setTextAnnotTextBoxRectMessage !== undefined);
    chrome.test.assertEq(
        'setTextAnnotTextBoxRect', setTextAnnotTextBoxRectMessage.type);
    chrome.test.assertTrue(
        chrome.test.checkDeepEq(newRect, setTextAnnotTextBoxRectMessage.data));
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
