// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Simulates a pointer press (pointerdown followed by pointerup, without a
 * `click` event) at the center of `target` to exercise PressHandler logic.
 * `options` can override default PointerEventInit fields (such as `button`,
 * `clientX`, `clientY`, or `detail`).
 */
export function simulatePointerPress(
    target: Element, options?: PointerEventInit): void {
  const rect = target.getBoundingClientRect();
  const eventInit: PointerEventInit = {
    button: 0,
    // Blink's default active mouse pointer ID (PointerEventFactory::kMouseId);
    // other IDs cause PressHandler.onPointerdown's setPointerCapture() call to
    // throw NotFoundError in suites that do not stub setPointerCapture().
    pointerId: 1,
    pointerType: 'mouse',
    clientX: rect.left + rect.width / 2,
    clientY: rect.top + rect.height / 2,
    bubbles: true,
    composed: true,
    ...options,
  };
  target.dispatchEvent(new PointerEvent('pointerdown', eventInit));
  target.dispatchEvent(new PointerEvent('pointerup', eventInit));
}
