// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {ContinueSignatureStrokeMessage, FinishSignatureStrokeMessage, StartSignatureStrokeMessage, ViewerSignatureDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

function getSignatureDialog(): ViewerSignatureDialogElement|null {
  return viewer.shadowRoot.querySelector<ViewerSignatureDialogElement>(
      'viewer-signature-dialog');
}

async function openSignatureDialog(): Promise<ViewerSignatureDialogElement> {
  const whenOpen = eventToPromise('cr-dialog-open', viewer);
  viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
  await whenOpen;
  await microtasksFinished();
  const dialog = getSignatureDialog();
  assertTrue(!!dialog);
  return dialog;
}

// Returns the RGBA color components [r, g, b, a] of the canvas pixel at the
// specified CSS coordinates (x, y), scaled by devicePixelRatio.
function getPixelColor(
    context: CanvasRenderingContext2D, x: number, y: number): number[] {
  const px = Math.round(x * window.devicePixelRatio);
  const py = Math.round(y * window.devicePixelRatio);
  return Array.from(context.getImageData(px, py, 1, 1).data);
}

// Creates a PointerEvent with a mocked `timeStamp`, since `Event.timeStamp` is
// read-only and cannot be set via `PointerEventInit`.
function createPointerEvent(
    type: string, init: PointerEventInit = {},
    timeStamp: number = 0): PointerEvent {
  const event = new PointerEvent(type, init);
  Object.defineProperty(event, 'timeStamp', {value: timeStamp});
  return event;
}

suite('Ink2SignatureDialogTest', function() {
  setup(async function() {
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    mockPlugin.clearMessages();
  });

  test('OpenAndCloseSignatureDialog', async function() {
    assertEquals(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    assertFalse(!!getSignatureDialog());

    // Entering signature mode opens the signature dialog.
    let dialog = await openSignatureDialog();
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
    assertTrue(dialog.$.dialog.open);
    assertTrue(isVisible(dialog.$.canvas));
    assertTrue(isVisible(dialog.$.clear));
    assertTrue(isVisible(dialog.$.create));
    assertTrue(dialog.$.clear.disabled);
    assertTrue(dialog.$.create.disabled);

    // Clicking the close button closes the dialog while staying in signature
    // mode.
    const closeButton =
        dialog.$.dialog.shadowRoot.querySelector<HTMLElement>('#close');
    assertTrue(!!closeButton);
    const whenClosed = eventToPromise('close', dialog);
    closeButton.click();
    await whenClosed;
    await microtasksFinished();

    assertFalse(!!getSignatureDialog());
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);

    // Exiting and re-entering signature mode opens the dialog again.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    assertFalse(!!getSignatureDialog());

    dialog = await openSignatureDialog();
    assertTrue(dialog.$.dialog.open);
  });

  test('IgnoreInvalidPointerEvents', async function() {
    const dialog = await openSignatureDialog();
    const canvas = dialog.$.canvas;

    // Non-left pointerdown and pointer events while not drawing are ignored.
    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {button: 1, clientX: 50, clientY: 50}));
    canvas.dispatchEvent(
        createPointerEvent('pointermove', {clientX: 60, clientY: 60}));
    canvas.dispatchEvent(
        createPointerEvent('pointerup', {clientX: 60, clientY: 60}));
    canvas.dispatchEvent(createPointerEvent('pointercancel'));
    assertEquals(undefined, mockPlugin.findMessage('startSignatureStroke'));
  });

  test('DrawSignature', async function() {
    const dialog = await openSignatureDialog();
    const canvas = dialog.$.canvas;
    assertTrue(canvas.width > 0);
    assertTrue(canvas.height > 0);
    const context = canvas.getContext('2d', {willReadFrequently: true})!;

    const firstOutline = [[10, 10, 30, 10, 30, 30, 10, 30]];
    mockPlugin.setMessageReply(
        'startSignatureStroke', {outlines: firstOutline});
    mockPlugin.setMessageReply(
        'continueSignatureStroke', {outlines: firstOutline});
    mockPlugin.setMessageReply(
        'finishSignatureStroke', {outlines: firstOutline});

    // Drawing a stroke sends start/continue/finish messages and renders the
    // returned outlines.
    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {pointerId: 1, clientX: 50, clientY: 50}, 100));
    await microtasksFinished();
    const startMessage = mockPlugin.findMessage<StartSignatureStrokeMessage>(
        'startSignatureStroke');
    assertTrue(!!startMessage);
    assertEquals(0, startMessage.point.elapsedTime);

    canvas.dispatchEvent(
        createPointerEvent('pointermove', {clientX: 70, clientY: 60}, 110));
    await microtasksFinished();
    const continueMessage =
        mockPlugin.findMessage<ContinueSignatureStrokeMessage>(
            'continueSignatureStroke');
    assertTrue(!!continueMessage);
    assertEquals(1, continueMessage.points.length);
    assertTrue(!!continueMessage.points[0]);
    assertEquals(10, continueMessage.points[0].elapsedTime);
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 20, 20));
    assertDeepEquals([0, 0, 0, 0], getPixelColor(context, 50, 20));

    canvas.dispatchEvent(
        createPointerEvent('pointerup', {clientX: 90, clientY: 70}, 125));
    await microtasksFinished();
    const finishMessage = mockPlugin.findMessage<FinishSignatureStrokeMessage>(
        'finishSignatureStroke');
    assertTrue(!!finishMessage);
    assertTrue(!!finishMessage.point);
    assertEquals(25, finishMessage.point.elapsedTime);
    assertFalse(dialog.$.clear.disabled);
    assertFalse(dialog.$.create.disabled);
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 20, 20));
    assertDeepEquals([0, 0, 0, 0], getPixelColor(context, 50, 20));

    // Starting a second stroke renders both the finished and in-progress
    // strokes.
    const secondOutline = [[40, 10, 60, 10, 60, 30, 40, 30]];
    mockPlugin.setMessageReply(
        'startSignatureStroke', {outlines: secondOutline});
    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {pointerId: 1, clientX: 50, clientY: 50}, 200));
    await microtasksFinished();
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 20, 20));
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 50, 20));
  });

  test('DrawSignaturePointerCancel', async function() {
    const dialog = await openSignatureDialog();
    const canvas = dialog.$.canvas;
    const context = canvas.getContext('2d', {willReadFrequently: true})!;

    const sampleOutline = [[10, 10, 30, 10, 30, 30, 10, 30]];
    mockPlugin.setMessageReply(
        'startSignatureStroke', {outlines: sampleOutline});
    mockPlugin.setMessageReply(
        'finishSignatureStroke', {outlines: sampleOutline});

    // Finishing a stroke via pointercancel sends finishSignatureStroke without
    // a point and renders the returned outlines.
    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {pointerId: 1, clientX: 50, clientY: 50}));
    canvas.dispatchEvent(createPointerEvent('pointercancel'));
    await microtasksFinished();
    const finishMessage = mockPlugin.findMessage<FinishSignatureStrokeMessage>(
        'finishSignatureStroke');
    assertTrue(!!finishMessage);
    assertEquals(undefined, finishMessage.point);
    assertFalse(dialog.$.clear.disabled);
    assertFalse(dialog.$.create.disabled);
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 20, 20));
  });

  test('ClearSignature', async function() {
    const dialog = await openSignatureDialog();
    const canvas = dialog.$.canvas;
    const context = canvas.getContext('2d', {willReadFrequently: true})!;

    const sampleOutline = [[10, 10, 30, 10, 30, 30, 10, 30]];
    mockPlugin.setMessageReply(
        'startSignatureStroke', {outlines: sampleOutline});
    mockPlugin.setMessageReply(
        'finishSignatureStroke', {outlines: sampleOutline});

    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {pointerId: 1, clientX: 50, clientY: 50}));
    canvas.dispatchEvent(
        createPointerEvent('pointerup', {clientX: 90, clientY: 70}));
    await microtasksFinished();
    assertFalse(dialog.$.clear.disabled);
    assertFalse(dialog.$.create.disabled);
    assertDeepEquals([227, 227, 227, 255], getPixelColor(context, 20, 20));

    // Clicking Clear clears the canvas and disables Clear and Create.
    dialog.$.clear.click();
    await microtasksFinished();
    assertTrue(dialog.$.clear.disabled);
    assertTrue(dialog.$.create.disabled);
    assertDeepEquals([0, 0, 0, 0], getPixelColor(context, 20, 20));
  });

  test('CreateSignature', async function() {
    const dialog = await openSignatureDialog();
    const canvas = dialog.$.canvas;

    const sampleOutline = [[10, 10, 30, 10, 30, 30, 10, 30]];
    mockPlugin.setMessageReply(
        'startSignatureStroke', {outlines: sampleOutline});
    mockPlugin.setMessageReply(
        'finishSignatureStroke', {outlines: sampleOutline});

    canvas.dispatchEvent(createPointerEvent(
        'pointerdown', {pointerId: 1, clientX: 50, clientY: 50}));
    canvas.dispatchEvent(
        createPointerEvent('pointerup', {clientX: 90, clientY: 70}));
    await microtasksFinished();
    assertFalse(dialog.$.create.disabled);

    // Clicking Create closes the dialog while staying in signature mode.
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.create.click();
    await whenClosed;
    await microtasksFinished();
    assertFalse(!!getSignatureDialog());
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
  });
});
