// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pinchtest = (function() {
  'use strict';

  function assertTrue(condition, message) {
    if (!condition) {
      message = message || "Assertion failed";
      console.trace();
      throw new Error(message);
    }
  }

  function assertClose(a, b, message) {
    if (Math.abs(a-b) > 1e-5) {
      message = message || "Assertion failed";
      console.log('"', a, '" and "', b, '" are not close.');
      console.trace();
      throw new Error(message);
    }
  }

  function isEquivalent(a, b) {
    // Create arrays of property names
    var aProps = Object.getOwnPropertyNames(a);
    var bProps = Object.getOwnPropertyNames(b);

    // If number of properties is different,
    // objects are not equivalent
    if (aProps.length != bProps.length) {
      return false;
    }

    for (var i = 0; i < aProps.length; i++) {
      var propName = aProps[i];

      // If values of same property are not equal,
      // objects are not equivalent
      if (a[propName] !== b[propName]) {
        return false;
      }
    }

    // If we made it this far, objects
    // are considered equivalent
    return true;
  }

  function assertEqual(a, b, message) {
    if (!isEquivalent(a, b)) {
      message = message || "Assertion failed";
      console.log('"', a, '" and "', b, '" are not equal');
      console.trace();
      throw new Error(message);
    }
  }

  var touch = (function() {
    'use strict';
    var points = {};
    function lowestID() {
      var ans = -1;
      for(var key in points) {
        ans = Math.max(ans, key);
      }
      return ans + 1;
    }
    function changeTouchPoint (key, x, y, offsetX, offsetY) {
      var e = {
        clientX: x,
        clientY: y,
        pageX: x,
        pageY: y
      };
      if (typeof(offsetX) === 'number') {
        e.clientX += offsetX;
      }
      if (typeof(offsetY) === 'number') {
        e.clientY += offsetY;
      }
      points[key] = e;
    }
    return {
      addTouchPoint: function(x, y, offsetX, offsetY) {
        changeTouchPoint(lowestID(), x, y, offsetX, offsetY);
      },
      updateTouchPoint: changeTouchPoint,
      releaseTouchPoint: function(key) {
        delete points[key];
      },
      events: function() {
        var arr = [];
        for(var key in points) {
          arr.push(points[key]);
        }
        return {
          touches: arr,
          preventDefault: function(){}
        };
      }
    };
  });

  function testZoomOut() {
    pincher.reset();
    var t = new touch();

    // Make sure start event doesn't change state
    var oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());

    // Make sure extra move event doesn't change state
    pincher.handleTouchMove(t.events());
    assertEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale < 0.9);

    // Make sure end event doesn't change state
    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
  }

  function testZoomIn() {
    pincher.reset();
    var t = new touch();

    var oldState = pincher.status();
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale > 1.1);

    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
  }

  function testZoomOutAndPan() {
    pincher.reset();
    var t = new touch();
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

    assertClose(pincher.status().shiftX, 10);
    assertClose(pincher.status().shiftY, -5);
    assertTrue(pincher.status().clampedScale < 0.9);
  }

  function testReversible() {
    pincher.reset();
    var t = new touch();
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
    assertClose(pincher.status().clampedScale, 1);
  }

  function testMultitouchZoomOut() {
    pincher.reset();
    var t = new touch();

    var oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(100, 300);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(300, 100);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());

    // Multi-touch zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    t.updateTouchPoint(2, 150, 250);
    t.updateTouchPoint(3, 250, 150);
    pincher.handleTouchMove(t.events());

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());

    assertTrue(pincher.status().clampedScale < 0.9);
  }

  function testZoomOutThenMulti() {
    pincher.reset();
    var t = new touch();

    var oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());

    // Zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale < 0.9);

    // Make sure adding and removing more point doesn't change state
    oldState = pincher.status();
    t.addTouchPoint(600, 600);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());

    // More than two fingers.
    t.addTouchPoint(150, 250);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 150);
    pincher.handleTouchStart(t.events());
    assertEqual(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    t.updateTouchPoint(2, 100, 300);
    t.updateTouchPoint(3, 300, 100);
    pincher.handleTouchMove(t.events());
    assertClose(pincher.status().scale, 1);

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    t.releaseTouchPoint(2);
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    assertEqual(oldState, pincher.status());
  }

  function testCancel() {
    pincher.reset();
    var t = new touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale < 0.9);

    var oldState = pincher.status();
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchCancel(t.events());
    assertEqual(oldState, pincher.status());

    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    assertClose(pincher.status().clampedScale, 1);
  }

  function testSingularity() {
    pincher.reset();
    var t = new touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 50, 50);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale > 1.1);
    assertTrue(pincher.status().clampedScale < 100);
    assertTrue(pincher.status().scale < 100);

    pincher.handleTouchCancel();
  }

  function testMinSpan() {
    pincher.reset();
    var t = new touch();

    t.addTouchPoint(50, 50);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 100, 100);
    pincher.handleTouchMove(t.events());
    assertTrue(pincher.status().clampedScale < 0.9);
    assertTrue(pincher.status().clampedScale > 0);
    assertTrue(pincher.status().scale > 0);

    pincher.handleTouchCancel();
  }

  function testFontScaling() {
    pincher.reset();
    useFontScaling(1.5);
    assertClose(pincher.status().clampedScale, 1.5);

    var t = new touch();

    // Start touch.
    var oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());

    // Pinch to zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());

    // Verify scale is smaller.
    assertTrue(pincher.status().clampedScale < 0.9 * oldState.clampedScale);
    pincher.handleTouchCancel();

    useFontScaling(0.8);
    assertClose(pincher.status().clampedScale, 0.8);

    // Start touch.
    t = new touch();
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
    assertTrue(pincher.status().clampedScale > 1.1 * oldState.clampedScale);
    pincher.handleTouchCancel();
  }

  return {
    run: function(){
      testZoomOut();
      testZoomIn();
      testZoomOutAndPan();
      testReversible();
      testMultitouchZoomOut();
      testZoomOutThenMulti();
      testCancel();
      testSingularity();
      testMinSpan();
      testFontScaling();
      pincher.reset();

      return {success: true};
    }
  };
}());
