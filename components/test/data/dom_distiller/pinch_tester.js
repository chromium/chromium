// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40108835): Consider replacing this class with
// dispatched touch events.
class Touch {
  constructor() {
    this.points = {};
  }

  /** @private */
  lowestID_() {
    let ans = -1;
    for (const key in this.points) {
      ans = Math.max(ans, key);
    }
    return ans + 1;
  }

  updateTouchPoint(key, x, y, offsetX, offsetY) {
    const e = {clientX: x, clientY: y, pageX: x, pageY: y};
    if (typeof (offsetX) === 'number') {
      e.clientX += offsetX;
    }
    if (typeof (offsetY) === 'number') {
      e.clientY += offsetY;
    }
    this.points[key] = e;
  }

  addTouchPoint(x, y, offsetX, offsetY) {
    this.updateTouchPoint(this.lowestID_(), x, y, offsetX, offsetY);
  }

  releaseTouchPoint(key) {
    delete this.points[key];
  }

  events() {
    const arr = [];
    for (const key in this.points) {
      arr.push(this.points[key]);
    }
    return {touches: arr, preventDefault: function() {}};
  }
}

suite('Pincher', function() {
  let chaiAssert = null;

  suiteSetup(async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./chai.js');
    chaiAssert = assert;
  });

  test('Zoom Out', function() {
    pincher.reset();
    const t = new Touch();

    // Make sure start event doesn't change state
    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    // Make sure extra move event doesn't change state
    pincher.handleTouchMove(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);

    // Make sure end event doesn't change state
    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
  });

  test('Zoom In', function() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    chaiAssert.isAbove(pincher.status().clampedScale, 1.1);

    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
  });

  test('Zomm Out And Pan', function() {
    pincher.reset();
    const t = new Touch();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    t.updateTouchPoint(0, 150, 150, 10, -5);
    t.updateTouchPoint(1, 250, 250, 10, -5);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());

    chaiAssert.closeTo(pincher.status().shiftX, 10, 1e-5);
    chaiAssert.closeTo(pincher.status().shiftY, -5, 1e-5);
    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);
  });

  test('Reversible', function() {
    pincher.reset();
    const t = new Touch();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 0, 0);
    t.updateTouchPoint(1, 400, 400);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    t.addTouchPoint(0, 0);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(400, 400);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    chaiAssert.closeTo(pincher.status().clampedScale, 1, 1e-5);
  });

  test('Multitouch Zoom Out', function() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(100, 300);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(300, 100);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    // Multi-touch zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    t.updateTouchPoint(2, 150, 250);
    t.updateTouchPoint(3, 250, 150);
    pincher.handleTouchMove(t.events());

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);
  });

  test('Zoom Out Then Multi', function() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    // Zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);

    // Make sure adding and removing more point doesn't change state
    oldState = pincher.status();
    t.addTouchPoint(600, 600);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    // More than two fingers.
    t.addTouchPoint(150, 250);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 150);
    pincher.handleTouchStart(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    t.updateTouchPoint(2, 100, 300);
    t.updateTouchPoint(3, 300, 100);
    pincher.handleTouchMove(t.events());
    chaiAssert.closeTo(pincher.status().scale, 1, 1e-5);

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    t.releaseTouchPoint(2);
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());
  });

  test('Cancel', function() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);

    const oldState = pincher.status();
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchCancel(t.events());
    chaiAssert.deepEqual(oldState, pincher.status());

    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    chaiAssert.closeTo(pincher.status().clampedScale, 1, 1e-5);
  });

  test('Singularity', function() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 50, 50);
    pincher.handleTouchMove(t.events());
    chaiAssert.isAbove(pincher.status().clampedScale, 1.1);
    chaiAssert.isBelow(pincher.status().clampedScale, 100);
    chaiAssert.isBelow(pincher.status().scale, 100);

    pincher.handleTouchCancel();
  });

  test('Min Span', function() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(50, 50);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 100, 100);
    pincher.handleTouchMove(t.events());
    chaiAssert.isBelow(pincher.status().clampedScale, 0.9);
    chaiAssert.isAbove(pincher.status().clampedScale, 0);
    chaiAssert.isAbove(pincher.status().scale, 0);

    pincher.handleTouchCancel();
  });

  test('Font Scaling', function() {
    pincher.reset();
    useFontScaling(1.5);
    chaiAssert.closeTo(pincher.status().clampedScale, 1.5, 1e-5);

    let t = new Touch();

    // Start touch.
    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());

    // Pinch to zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());

    // Verify scale is smaller.
    chaiAssert.isBelow(
        pincher.status().clampedScale, 0.9 * oldState.clampedScale);
    pincher.handleTouchCancel();

    useFontScaling(0.8);
    chaiAssert.closeTo(pincher.status().clampedScale, 0.8, 1e-5);

    // Start touch.
    t = new Touch();
    oldState = pincher.status();
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());

    // Pinch to zoom in.
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());

    // Verify scale is larger.
    chaiAssert.isAbove(
        pincher.status().clampedScale, 1.1 * oldState.clampedScale);
    pincher.handleTouchCancel();
  });
});
