// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

function createPointerEvent(eventType: string, point: Point): PointerEvent {
  return new PointerEvent(eventType, {
    pointerId: 1,
    bubbles: true,
    button: 0,
    clientX: point.x,
    clientY: point.y,
    isPrimary: true,
  });
}

export function getImageBoundingRect(
    selectionOverlayElement: SelectionOverlayElement) {
  return selectionOverlayElement.$.backgroundImage.getBoundingClientRect();
}

export function simulateClick(
    selectionOverlayElement: SelectionOverlayElement, point: Point) {
  const pointerDownEvent = createPointerEvent('pointerdown', point);
  const pointerUpEvent = createPointerEvent('pointerup', point);

  selectionOverlayElement.dispatchEvent(pointerDownEvent);
  selectionOverlayElement.dispatchEvent(pointerUpEvent);
  return flushTasks();
}

export function simulateDrag(
    selectionOverlayElement: SelectionOverlayElement, fromPoint: Point,
    toPoint: Point) {
  const pointerDownEvent = createPointerEvent('pointerdown', fromPoint);
  const pointerMoveEvent = createPointerEvent('pointermove', toPoint);
  const pointerUpEvent = createPointerEvent('pointerup', toPoint);

  selectionOverlayElement.dispatchEvent(pointerDownEvent);
  selectionOverlayElement.dispatchEvent(pointerMoveEvent);
  selectionOverlayElement.dispatchEvent(pointerUpEvent);
  return flushTasks();
}
