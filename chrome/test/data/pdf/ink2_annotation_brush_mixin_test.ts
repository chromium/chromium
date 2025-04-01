// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, hexToColor, HIGHLIGHTER_COLORS, HIGHLIGHTER_SIZES, Ink2Manager, InkAnnotationBrushMixin, PEN_COLORS, PEN_SIZES} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {setGetAnnotationBrushReply, setupTestMockPluginForInk} from './test_util.js';

const mockPlugin = setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

const TestElementBase = InkAnnotationBrushMixin(CrLitElement);

class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }

  override render() {
    return html`<div>Hello World</div>`;
  }
}

customElements.define(TestElement.is, TestElement);
const testElement = document.createElement('test-element') as TestElement;
document.body.appendChild(testElement);

chrome.test.runTests([
  async function testSetBrushProperties() {
    // Defaults
    chrome.test.assertEq(0, testElement.currentColor.r);
    chrome.test.assertEq(0, testElement.currentColor.b);
    chrome.test.assertEq(0, testElement.currentColor.g);
    chrome.test.assertEq(PEN_SIZES[0]!.size, testElement.currentSize);
    chrome.test.assertEq(AnnotationBrushType.PEN, testElement.currentType);

    // Verify that calling onColorChanged with a new hex color changes the
    // color.
    const newColor = hexToColor(PEN_COLORS[1]!.color);
    const colorEvent =
        new CustomEvent('current-color-changed', {detail: {value: newColor}});
    let whenChanged = eventToPromise('brush-changed', manager);
    testElement.onCurrentColorChanged(colorEvent);
    await whenChanged;
    chrome.test.assertEq(newColor.r, testElement.currentColor.r);
    chrome.test.assertEq(newColor.g, testElement.currentColor.g);
    chrome.test.assertEq(newColor.b, testElement.currentColor.b);

    // Verify that calling onSizeChanged with a new hex size changes the
    // color.
    const sizeEvent = new CustomEvent(
        'current-size-changed', {detail: {value: PEN_SIZES[1]!.size}});
    whenChanged = eventToPromise('brush-changed', manager);
    testElement.onCurrentSizeChanged(sizeEvent);
    await whenChanged;
    chrome.test.assertEq(PEN_SIZES[1]!.size, testElement.currentSize);

    // Verify that calling onTypeChanged updates the type.
    const typeEvent = new CustomEvent(
        'current-type-changed', {detail: {value: AnnotationBrushType.ERASER}});
    // Set the plugin response to eraser.
    setGetAnnotationBrushReply(mockPlugin, AnnotationBrushType.ERASER);
    whenChanged = eventToPromise('brush-changed', manager);
    testElement.onCurrentTypeChanged(typeEvent);
    await whenChanged;
    chrome.test.assertEq(AnnotationBrushType.ERASER, testElement.currentType);

    chrome.test.succeed();
  },

  async function testAvailableColors() {
    // Switch to pen.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.PEN, PEN_SIZES[1]!.size,
        hexToColor(PEN_COLORS[1]!.color));
    testElement.onCurrentTypeChanged(new CustomEvent(
        'current-type-changed', {detail: {value: AnnotationBrushType.PEN}}));
    await eventToPromise('brush-changed', manager);
    chrome.test.assertEq(AnnotationBrushType.PEN, testElement.currentType);

    // Confirm the available colors contain the expected values.
    const colors = testElement.availableBrushColors();
    chrome.test.assertEq(colors.length, PEN_COLORS.length);
    for (let i = 0; i < colors.length; i++) {
      chrome.test.assertEq(PEN_COLORS[i]!.label, colors[i]!.label);
      chrome.test.assertEq(PEN_COLORS[i]!.color, colors[i]!.color);
      chrome.test.assertEq(false, colors[i]!.blended);
    }

    // Switch to highlighter.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.HIGHLIGHTER, HIGHLIGHTER_SIZES[0]!.size,
        hexToColor(HIGHLIGHTER_COLORS[0]!.color));
    testElement.onCurrentTypeChanged(new CustomEvent(
        'current-type-changed',
        {detail: {value: AnnotationBrushType.HIGHLIGHTER}}));
    await eventToPromise('brush-changed', manager);
    chrome.test.assertEq(
        AnnotationBrushType.HIGHLIGHTER, testElement.currentType);

    // Confirm the available colors contain the expected values.
    const highlighterColors = testElement.availableBrushColors();
    chrome.test.assertEq(highlighterColors.length, HIGHLIGHTER_COLORS.length);
    for (let i = 0; i < highlighterColors.length; i++) {
      chrome.test.assertEq(
          HIGHLIGHTER_COLORS[i]!.label, highlighterColors[i]!.label);
      chrome.test.assertEq(
          HIGHLIGHTER_COLORS[i]!.color, highlighterColors[i]!.color);
      // Highlighter colors should indicate that they should be displayed
      // blended with white.
      chrome.test.assertTrue(!!highlighterColors[i]!.blended);
    }
    chrome.test.succeed();
  },
]);
