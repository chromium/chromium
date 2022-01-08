// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Point, ViewportScroller} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

class FakePlugin extends EventTarget {
  /** @return {number} */
  get offsetWidth() {
    return 400;
  }

  /** @return {number} */
  get offsetHeight() {
    return 300;
  }
}

class FakeViewport extends EventTarget {
  constructor() {
    super();

    /** @private {!Point} */
    this.position_ = {x: 0, y: 0};
  }

  /** @return {!Point} */
  get position() {
    return this.position_;
  }

  /** @param {!Point} position */
  setPosition(position) {
    this.position_ = position;
    this.dispatchEvent(new CustomEvent('scroll'));
  }
}

/**
 * Creates a synthetic "mousemove" event.
 * @param {number} offsetX
 * @param {number} offsetY
 * @return {!Event}
 */
function createMouseMoveEvent(offsetX, offsetY) {
  const event = new CustomEvent('mousemove');
  event.offsetX = offsetX;
  event.offsetY = offsetY;
  return event;
}

chrome.test.runTests([
  function testScrollUpAndLeft() {
    const viewport = new FakeViewport();
    const plugin = new FakePlugin();
    const scroller = new ViewportScroller(viewport, plugin, window);

    viewport.addEventListener('scroll', () => {
      scroller.setEnableScrolling(false);

      chrome.test.assertTrue(
          viewport.position.x < 0, `${viewport.position.x} >= 0`);
      chrome.test.assertTrue(
          viewport.position.y < 0, `${viewport.position.y} >= 0`);
      chrome.test.succeed();
    });

    scroller.setEnableScrolling(true);
    plugin.dispatchEvent(createMouseMoveEvent(-10, -20));
  },

  function testScrollDownAndRight() {
    const viewport = new FakeViewport();
    const plugin = new FakePlugin();
    const scroller = new ViewportScroller(viewport, plugin, window);

    viewport.addEventListener('scroll', () => {
      scroller.setEnableScrolling(false);

      chrome.test.assertTrue(
          viewport.position.x > 0, `${viewport.position.x} <= 0`);
      chrome.test.assertTrue(
          viewport.position.y > 0, `${viewport.position.y} <= 0`);
      chrome.test.succeed();
    });

    scroller.setEnableScrolling(true);
    plugin.dispatchEvent(createMouseMoveEvent(
        plugin.offsetWidth + 10, plugin.offsetHeight + 20));
  },
]);
