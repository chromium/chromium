// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Point, ViewportInterface, ViewportScroller} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

class FakePlugin extends EventTarget {
  get offsetWidth(): number {
    return 400;
  }

  get offsetHeight(): number {
    return 300;
  }
}

class FakeViewport extends EventTarget implements ViewportInterface {
  private position_: Point = {x: 0, y: 0};

  get position(): Point {
    return this.position_;
  }

  setPosition(position: Point) {
    this.position_ = position;
    this.dispatchEvent(new CustomEvent('scroll'));
  }
}

type FakeEvent = Event&{offsetX: number, offsetY: number};

/** Creates a synthetic "mousemove" event. */
function createMouseMoveEvent(offsetX: number, offsetY: number): Event {
  // Note: Not using MouseEvent() because |offsetX| and |offsetY| are read-only
  // on MouseEvent.
  const event = new Event('mousemove') as FakeEvent;
  event.offsetX = offsetX;
  event.offsetY = offsetY;
  return event;
}

chrome.test.runTests([
  function testScrollUpAndLeft() {
    const viewport = new FakeViewport();
    const plugin = new FakePlugin() as HTMLElement;
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
    const plugin = new FakePlugin() as HTMLElement;
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
