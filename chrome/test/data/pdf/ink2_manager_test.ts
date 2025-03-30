// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrush, AnnotationText} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, Ink2Manager, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';

import {assertAnnotationBrush, setGetAnnotationBrushReply, setupTestMockPluginForInk} from './test_util.js';

const mockPlugin = setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

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
        assert(brushUpdates[index]!.color);
        chrome.test.assertEq(expected.color.r, brushUpdates[index]!.color.r);
        chrome.test.assertEq(expected.color.g, brushUpdates[index]!.color.g);
        chrome.test.assertEq(expected.color.b, brushUpdates[index]!.color.b);
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

  function testSetTextProperties() {
    const textUpdates: AnnotationText[] = [];
    manager.addEventListener('text-changed', e => {
      textUpdates.push((e as CustomEvent<AnnotationText>).detail);
    });

    function assertTextUpdate(index: number, expected: AnnotationText) {
      chrome.test.assertEq(index + 1, textUpdates.length);
      chrome.test.assertEq(expected.font, textUpdates[index]!.font);
      chrome.test.assertEq(expected.size, textUpdates[index]!.size);
      chrome.test.assertEq(expected.color.r, textUpdates[index]!.color.r);
      chrome.test.assertEq(expected.color.g, textUpdates[index]!.color.g);
      chrome.test.assertEq(expected.color.b, textUpdates[index]!.color.b);
      chrome.test.assertEq(expected.alignment, textUpdates[index]!.alignment);
      chrome.test.assertEq(
          expected.styles.bold, textUpdates[index]!.styles.bold);
      chrome.test.assertEq(
          expected.styles.italic, textUpdates[index]!.styles.italic);
      chrome.test.assertEq(
          expected.styles.underline, textUpdates[index]!.styles.underline);
      chrome.test.assertEq(
          expected.styles.strikethrough,
          textUpdates[index]!.styles.strikethrough);
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
    assertTextUpdate(0, expectedText);

    // Set size to 10.
    manager.setTextSize(10);
    expectedText.size = 10;
    assertTextUpdate(1, expectedText);

    // Set alignment to CENTER.
    manager.setTextAlignment(TextAlignment.CENTER);
    expectedText.alignment = TextAlignment.CENTER;
    assertTextUpdate(2, expectedText);

    // Set color to blue.
    const blue = {r: 0, b: 100, g: 0};
    manager.setTextColor(blue);
    expectedText.color = blue;
    assertTextUpdate(3, expectedText);

    // Set style to bold + italic.
    const boldItalic =
        {bold: true, italic: true, underline: false, strikethrough: false};
    manager.setTextStyles(boldItalic);
    expectedText.styles = boldItalic;
    assertTextUpdate(4, expectedText);

    chrome.test.succeed();
  },
]);
