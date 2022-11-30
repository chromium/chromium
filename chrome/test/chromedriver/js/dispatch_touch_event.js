// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dispatches a fake touch event with a single touch point.
 *
 * @param {number} x X-coordinate of the touch point relative to viewport
 *     in CSS pixels.
 * @param {number} y Y-coordinate of the touch point relative to viewport
 *     in CSS pixels.
 * @param {string} type Touch event type, e.g. touchstart.
 */
function dispatchTouchEvent(x, y, type) {
  var element = window.document.elementFromPoint(x, y);
  var inputDeviceCapabilities =
    new InputDeviceCapabilities({firesTouchEvents: true});
  var touch = new Touch({
    identifier: 0,
    target: element,
    clientX: x,
    clientY: y,
    pageX: x + window.document.scrollingElement.scrollLeft,
    pageY: y + window.document.scrollingElement.scrollTop,
    force: 1,
    radiusX: 1,
    radiusY: 1,
    screenX: x + window.screenX,
    screenY: y + window.screenY,
  });
  var event = new TouchEvent(type, {
    touches: [touch],
    targetTouches: [touch],
    changedTouches: [touch],
    ctrlKey: false,
    shiftKey: false,
    altKey: false,
    metaKey: false,
    view: window,
    bubbles: true,
    cancelable: false,
    composed: true,
    sourceCapabilities: inputDeviceCapabilities,
  });
  element.dispatchEvent(event);
}
