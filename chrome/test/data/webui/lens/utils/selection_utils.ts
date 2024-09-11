// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

function createPointerEvent(
    eventType: string, point: Point, button = 0): PointerEvent {
  return new PointerEvent(eventType, {
    pointerId: 1,
    bubbles: true,
    button,
    clientX: point.x,
    clientY: point.y,
    isPrimary: true,
  });
}

export function getImageBoundingRect(
    selectionOverlayElement: SelectionOverlayElement) {
  return selectionOverlayElement.$.backgroundImageCanvas
      .getBoundingClientRect();
}

export function simulateClick(
    selectionOverlayElement: SelectionOverlayElement, point: Point,
    button = 0) {
  const pointerDownEvent = createPointerEvent('pointerdown', point, button);
  const pointerUpEvent = createPointerEvent('pointerup', point, button);

  selectionOverlayElement.dispatchEvent(pointerDownEvent);
  selectionOverlayElement.dispatchEvent(pointerUpEvent);
  return flushTasks();
}

export async function simulateDrag(
    selectionOverlayElement: SelectionOverlayElement, fromPoint: Point,
    toPoint: Point) {
  await simulateStartDrag(selectionOverlayElement, fromPoint, toPoint);

  const pointerUpEvent = createPointerEvent('pointerup', toPoint);
  selectionOverlayElement.dispatchEvent(pointerUpEvent);
  return flushTasks();
}

export function simulateStartDrag(
    selectionOverlayElement: SelectionOverlayElement, fromPoint: Point,
    toPoint: Point) {
  const pointerDownEvent = createPointerEvent('pointerdown', fromPoint);
  const pointerMoveEvent = createPointerEvent('pointermove', toPoint);

  selectionOverlayElement.dispatchEvent(pointerDownEvent);
  selectionOverlayElement.dispatchEvent(pointerMoveEvent);

  // Since pointer move responds once per frame, we need to render a frame
  // instead of just relying on flushTasks.
  return waitAfterNextRender(selectionOverlayElement);
}
