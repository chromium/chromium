// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, AnnotationText} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, Ink2Manager, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertDeepEquals, setGetAnnotationBrushReply, setupTestMockPluginForInk} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

const mockPlugin = setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

/**
 * Tests that the current annotation text matches `expectedText`. Clears all
 * messages from `mockPlugin` after, otherwise subsequent calls would continue
 * to find and use the same message.
 * @param mockPlugin The mock plugin receiving messages.
 * @param expectedText The expected text that the current annotation text
 * should match.
 */
export function assertAnnotationText(
    mockPlugin: MockPdfPluginElement, expectedText: AnnotationText) {
  const setAnnotationTextMessage =
      mockPlugin.findMessage('setTextAnnotationFont');
  chrome.test.assertTrue(setAnnotationTextMessage !== undefined);
  chrome.test.assertEq('setTextAnnotationFont', setAnnotationTextMessage.type);
  chrome.test.assertEq(
      expectedText.font, setAnnotationTextMessage.data.typeface);
  chrome.test.assertEq(
      expectedText.size, setAnnotationTextMessage.data.fontSize);
  chrome.test.assertEq(
      expectedText.alignment, setAnnotationTextMessage.data.alignment);
  assertDeepEquals(expectedText.color, setAnnotationTextMessage.data.color);
  assertDeepEquals(expectedText.styles, setAnnotationTextMessage.data.style);

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

  async function testGetTextAnnotationFonts() {
    // Checks that requesting the fonts for the first time retrieves the
    // fonts from the plugin and fires a text-changed event with the font
    // set to the first font returned.
    const whenChanged = eventToPromise('text-changed', manager);
    const fonts = await manager.getTextAnnotationFonts();

    // For now, these are hardcoded in controller.ts.
    const expectedFonts = ['Roboto', 'Serif', 'Sans', 'Monospace'];
    chrome.test.assertEq(fonts.length, expectedFonts.length);
    for (let i = 0; i < expectedFonts.length; i++) {
      chrome.test.assertEq(fonts[i], expectedFonts[i]);
    }

    // Check that the manager requested the fonts.
    const getTextAnnotFontNamesMessage =
        mockPlugin.findMessage('getTextAnnotFontNames');
    chrome.test.assertTrue(getTextAnnotFontNamesMessage !== undefined);
    chrome.test.assertEq(
        'getTextAnnotFontNames', getTextAnnotFontNamesMessage.type);

    // Check that an event was fired.
    const changedEvent = await whenChanged;
    chrome.test.assertEq('Roboto', changedEvent.detail.font);
    // Manager should also have sent the initial text settings to the plugin.
    const expectedText = {
      font: 'Roboto',
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
    assertAnnotationText(mockPlugin, expectedText);
    chrome.test.succeed();
  },

  function testSetTextProperties() {
    const textUpdates: AnnotationText[] = [];
    manager.addEventListener('text-changed', e => {
      textUpdates.push((e as CustomEvent<AnnotationText>).detail);
    });

    function assertTextUpdate(index: number, expected: AnnotationText) {
      chrome.test.assertEq(index + 1, textUpdates.length);
      chrome.test.assertEq(expected.font, textUpdates[index]!.font);
      chrome.test.assertEq(expected.size, textUpdates[index]!.size);
      assertDeepEquals(expected.color, textUpdates[index]!.color);
      chrome.test.assertEq(expected.alignment, textUpdates[index]!.alignment);
      assertDeepEquals(expected.styles, textUpdates[index]!.styles);
    }

    // Update font. Note the other `expectedText` values come from the defaults
    // set in ink2_manager.ts.
    manager.setTextFont('Serif');
    const expectedText = {
      font: 'Serif',
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
    assertAnnotationText(mockPlugin, expectedText);
    assertTextUpdate(0, expectedText);

    // Set size to 10.
    manager.setTextSize(10);
    expectedText.size = 10;
    assertAnnotationText(mockPlugin, expectedText);
    assertTextUpdate(1, expectedText);

    // Set alignment to CENTER.
    manager.setTextAlignment(TextAlignment.CENTER);
    expectedText.alignment = TextAlignment.CENTER;
    assertAnnotationText(mockPlugin, expectedText);
    assertTextUpdate(2, expectedText);

    // Set color to blue.
    const blue = {r: 0, b: 100, g: 0};
    manager.setTextColor(blue);
    expectedText.color = blue;
    assertAnnotationText(mockPlugin, expectedText);
    assertTextUpdate(3, expectedText);

    // Set style to bold + italic.
    const boldItalic =
        {bold: true, italic: true, underline: false, strikethrough: false};
    manager.setTextStyles(boldItalic);
    expectedText.styles = boldItalic;
    assertAnnotationText(mockPlugin, expectedText);
    assertTextUpdate(4, expectedText);

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
]);
