// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, PdfViewerPrivateProxyImpl, TEXT_COLORS, TextAlignment, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {TextAnnotation, TextAnnotationMessageData, TextBoxRect} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {assertDeepEquals, setUpInkTestContext} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

export function setupTextBoxTest() {
  const {viewport, mockPlugin} = setUpInkTestContext();
  const privateProxy = new TestPdfViewerPrivateProxy();
  PdfViewerPrivateProxyImpl.setInstance(privateProxy);
  const manager = Ink2Manager.getInstance();
  manager.initializeTextAnnotations();
  const textbox = document.createElement('ink-text-box');
  document.body.appendChild(textbox);
  return {viewport, mockPlugin, privateProxy, manager, textbox};
}

export function getTestAnnotation(textBoxRect: TextBoxRect): TextAnnotation {
  return {
    id: 0,
    mojoTextInfo: new ArrayBuffer(0),
    pageIndex: 0,
    text: 'Hello World',
    textAttributes: {
      alignment: TextAlignment.LEFT,
      color: hexToColor(TEXT_COLORS[0]!.color),
      size: 12,
      styles: {
        [TextStyle.BOLD]: false,
        [TextStyle.ITALIC]: false,
      },
      typeface: TextTypeface.SANS_SERIF,
    },
    textBoxRect,
    textOrientation: 0,
  };
}

export function initializeBox(
    manager: Ink2Manager, width: number, height: number, x: number, y: number,
    existing?: boolean, orientation?: number) {
  const annotation =
      getTestAnnotation({height, locationX: x, locationY: y, width});
  if (!existing) {
    annotation.text = '';
  }
  if (orientation) {
    annotation.textOrientation = orientation;
  }

  manager.dispatchEvent(new CustomEvent('initialize-text-box', {
    detail: {
      annotation,
      // Large width and height so we don't need to worry about size clamping
      // in tests where we don't want to explicitly validate it.
      pageDimensions: {x: 10, y: 3, width: 1000, height: 1000},
    },
  }));
}

export function assertPositionAndSize(
    el: HTMLElement, expectedWidth: string, expectedHeight: string,
    expectedLeft: string, expectedTop: string) {
  const styles = getComputedStyle(el);
  chrome.test.assertEq(expectedWidth, styles.getPropertyValue('width'));
  chrome.test.assertEq(expectedHeight, styles.getPropertyValue('height'));
  chrome.test.assertEq(expectedLeft, styles.getPropertyValue('left'));
  chrome.test.assertEq(expectedTop, styles.getPropertyValue('top'));
}

export async function dragHandle(
    handle: HTMLElement, deltaX: number, deltaY: number) {
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

export async function dragHandleWithKeyboard(
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

export function verifyFinishTextAnnotationMessage(
    mockPlugin: MockPdfPluginElement, expectedAnnotation: TextAnnotation,
    expectedIsEdited: boolean, expectedPdfZoom: number = 1.0) {
  const message =
      mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
          'finishTextAnnotation');
  chrome.test.assertTrue(message !== undefined);
  chrome.test.assertEq('finishTextAnnotation', message.type);
  const expectedMessageData: TextAnnotationMessageData = {
    ...expectedAnnotation,
    isEdited: expectedIsEdited,
    isUser: true,
    newTypefaces: [],
    pdfZoom: expectedPdfZoom,
  };
  assertDeepEquals(expectedMessageData, message.data);
}
