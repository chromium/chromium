// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, pageToScreenCoordinates, PdfViewerPrivateProxyImpl, TEXT_COLORS, TextAlignment, TextAnnotationSource, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkTextBoxElement, TextAnnotation, TextAnnotationMessageData, TextBoxRect, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isMac} from 'chrome://resources/js/platform.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {assertDeepEquals, MockDocumentDimensions, setUpInkTestContext} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

export function getCtrlModifier(): ModifiersParam {
  return isMac ? ['meta'] : ['ctrl'];
}

export function getStrikethroughModifiers(): ModifiersParam {
  return isMac ? ['meta', 'shift'] : ['alt', 'shift'];
}

export function getStrikethroughKey(): string {
  return isMac ? 'x' : '5';
}

export async function setupTextBoxTest(
    windowWidth: number = 500, windowHeight: number = 500,
    pageWidth: number = 400, pageHeight: number = 500,
    zeroScrollbars: boolean = false) {
  document.body.innerHTML = '';
  document.body.style.overflow = 'hidden';
  document.documentElement.style.overflow = 'hidden';

  Ink2Manager.setInstance(null);
  const {viewport, mockPlugin, mockWindow} =
      setUpInkTestContext(zeroScrollbars ? 0 : 5);

  mockWindow.offsetWidth = windowWidth;
  mockWindow.offsetHeight = windowHeight;
  if (mockWindow.resizeCallback) {
    mockWindow.resizeCallback();
  }

  const documentDimensions = new MockDocumentDimensions(0, 0);
  documentDimensions.addPage(pageWidth, pageHeight);
  viewport.setDocumentDimensions(documentDimensions);

  const privateProxy = new TestPdfViewerPrivateProxy();
  PdfViewerPrivateProxyImpl.setInstance(privateProxy);
  const manager = Ink2Manager.getInstance();
  await manager.initializeTextAnnotations();
  const textbox = document.createElement('ink-text-box');
  textbox.viewport = viewport;
  viewport.setViewportChangedCallback(() => textbox.viewportChanged());
  document.body.appendChild(textbox);
  return {viewport, mockPlugin, privateProxy, manager, textbox, mockWindow};
}

export function getTestAnnotation(
    textBoxRect: TextBoxRect, pdfZoom: number = 1.0): TextAnnotation {
  return {
    id: 0,
    mojoTextInfo: new ArrayBuffer(0),
    pageIndex: 0,
    pdfZoom,
    text: 'Hello World',
    textAttributes: {
      alignment: TextAlignment.LEFT,
      color: hexToColor(TEXT_COLORS[0]!.color),
      size: 12,
      styles: {
        [TextStyle.BOLD]: false,
        [TextStyle.ITALIC]: false,
        [TextStyle.STRIKETHROUGH]: false,
      },
      typeface: TextTypeface.SANS_SERIF,
    },
    textBoxRect,
    textOrientation: 0,
    viewportOrientation: 0,
  };
}

export function initializeBox(
    width: number, height: number, x: number, y: number, orientation?: number) {
  const textbox = document.body.querySelector('ink-text-box');
  chrome.test.assertTrue(!!textbox, 'ink-text-box element not found');

  const annotation =
      getTestAnnotation({height, locationX: x, locationY: y, width});
  annotation.text = '';
  if (orientation) {
    annotation.textOrientation = orientation;
  }

  textbox.annotation = annotation;
  // Large width and height so size does not clamp in tests that are not
  // intended to explicitly validate it.
  textbox.pageDimensions = {x: 10, y: 3, width: 1000, height: 1000};
}

export function assertPositionAndSize(
    el: HTMLElement, expectedWidth: string, expectedHeight: string,
    expectedLeft: string, expectedTop: string) {
  const styles = getComputedStyle(el);

  function parsePixelValue(str: string): number {
    // Extract the numerical part of the style string.
    const match = str.match(/^([+-]?\d+(\.\d+)?)px$/);
    chrome.test.assertTrue(match !== null, `Invalid pixel string: ${str}`);
    chrome.test.assertEq(3, match.length);
    return parseFloat(match[1]!);
  }

  function assertStylePixelValue(expectedString: string, actualString: string) {
    if (expectedString === 'auto' || actualString === 'auto') {
      chrome.test.assertEq(expectedString, actualString);
      return;
    }
    const expectedVal = parsePixelValue(expectedString);
    const actualVal = parsePixelValue(actualString);
    chrome.test.assertTrue(
        Math.abs(expectedVal - actualVal) < 1.0,
        `Expected ${expectedString}, but got ${actualString}`);
  }

  assertStylePixelValue(expectedWidth, styles.getPropertyValue('width'));
  assertStylePixelValue(expectedHeight, styles.getPropertyValue('height'));
  assertStylePixelValue(expectedLeft, styles.getPropertyValue('left'));
  assertStylePixelValue(expectedTop, styles.getPropertyValue('top'));
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
    expectedIsEdited: boolean) {
  const message =
      mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
          'finishTextAnnotation');
  chrome.test.assertTrue(message !== undefined);
  chrome.test.assertEq('finishTextAnnotation', message.type);
  const expectedMessageData: TextAnnotationMessageData = {
    ...expectedAnnotation,
    isEdited: expectedIsEdited,
    newTypefaces: [],
    source: TextAnnotationSource.USER,
  };
  assertDeepEquals(expectedMessageData, message.data);
}

/**
 * Simulates reactivating an existing text annotation for editing.
 * It converts the annotation's page-relative coordinates to screen coordinates
 * and sets the textbox's `annotation` and `pageDimensions` properties to
 * trigger the transition to the active/editing state.
 */
export function reactivateBox(
    textbox: InkTextBoxElement, viewport: Viewport,
    annotation: TextAnnotation) {
  const pageRect = viewport.getPageScreenRect(annotation.pageIndex);
  const screenRect = pageToScreenCoordinates(
      annotation.pageIndex, annotation.textBoxRect, viewport);
  const annotationToPass = structuredClone(annotation);
  annotationToPass.textBoxRect = screenRect;

  textbox.annotation = annotationToPass;
  textbox.pageDimensions = pageRect;
}
