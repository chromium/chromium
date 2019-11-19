// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GestureDetector} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/gesture_detector.js';

chrome.test.runTests(function() {
  'use strict';

  class StubElement {
    constructor() {
      this.listeners = new Map([
        ['touchstart', []],
        ['touchmove', []],
        ['touchend', []],
        ['touchcancel', []],
        ['wheel', []],
      ]);
    }

    addEventListener(type, listener, options) {
      if (this.listeners.has(type)) {
        this.listeners.get(type).push({listener: listener, options: options});
      }
    }

    sendEvent(event) {
      for (const l of this.listeners.get(event.type)) {
        l.listener(event);
      }
    }
  }

  class MockTouchEvent {
    constructor(type, touches) {
      this.type = type;
      this.touches = touches;
      this.defaultPrevented = false;
    }

    preventDefault() {
      this.defaultPrevented = true;
    }
  }

  class MockWheelEvent {
    constructor(deltaY, position, ctrlKey) {
      this.type = 'wheel';
      this.deltaY = deltaY;
      this.clientX = position.clientX;
      this.clientY = position.clientY;
      this.ctrlKey = ctrlKey;
      this.defaultPrevented = false;
    }

    preventDefault() {
      this.defaultPrevented = true;
    }
  }

  class PinchListener {
    constructor(gestureDetector) {
      this.lastEvent = null;
      gestureDetector.addEventListener('pinchstart', this.onPinch_.bind(this));
      gestureDetector.addEventListener('pinchupdate', this.onPinch_.bind(this));
      gestureDetector.addEventListener('pinchend', this.onPinch_.bind(this));
    }

    onPinch_(pinchEvent) {
      this.lastEvent = pinchEvent;
    }
  }

  return [
    function testPinchZoomIn() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      chrome.test.assertEq(
          {type: 'pinchstart', center: {x: 0, y: 1}}, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4},
      ]));
      chrome.test.assertEq(
          {
            type: 'pinchupdate',
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 2,
            center: {x: 0, y: 2}
          },
          pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 8},
      ]));
      chrome.test.assertEq(
          {
            type: 'pinchupdate',
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 4,
            center: {x: 0, y: 4}
          },
          pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq(
          {type: 'pinchend', startScaleRatio: 4, center: {x: 0, y: 4}},
          pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testPinchZoomInAndBackOut() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      chrome.test.assertEq(
          {type: 'pinchstart', center: {x: 0, y: 1}}, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4},
      ]));
      chrome.test.assertEq(
          {
            type: 'pinchupdate',
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 2,
            center: {x: 0, y: 2}
          },
          pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      // This should be part of the same gesture as an update.
      // A change in direction should not end the gesture and start a new one.
      chrome.test.assertEq(
          {
            type: 'pinchupdate',
            scaleRatio: 0.5,
            direction: 'out',
            startScaleRatio: 1,
            center: {x: 0, y: 1}
          },
          pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq(
          {type: 'pinchend', startScaleRatio: 1, center: {x: 0, y: 1}},
          pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testZoomWithWheel() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      // Since the wheel events that the GestureDetector receives are
      // individual updates without begin/end events, we need to make sure the
      // GestureDetector generates appropriate pinch begin/end events itself.
      class PinchSequenceListener {
        constructor(gestureDetector) {
          this.seenBegin = false;
          gestureDetector.addEventListener('pinchstart', function() {
            this.seenBegin = true;
          }.bind(this));
          this.endPromise = new Promise(function(resolve) {
            gestureDetector.addEventListener('pinchend', resolve);
          });
        }
      }
      const pinchSequenceListener = new PinchSequenceListener(gestureDetector);

      const scale = 1.23;
      const deltaY = -(100.0 * Math.log(scale));
      const position = {clientX: 12, clientY: 34};
      stubElement.sendEvent(new MockWheelEvent(deltaY, position, true));

      chrome.test.assertTrue(pinchSequenceListener.seenBegin);

      const lastEvent = pinchListener.lastEvent;
      chrome.test.assertEq('pinchupdate', lastEvent.type);
      chrome.test.assertTrue(Math.abs(lastEvent.scaleRatio - scale) < 0.001);
      chrome.test.assertEq('in', lastEvent.direction);
      chrome.test.assertTrue(
          Math.abs(lastEvent.startScaleRatio - scale) < 0.001);
      chrome.test.assertEq(
          {x: position.clientX, y: position.clientY}, lastEvent.center);

      pinchSequenceListener.endPromise.then(chrome.test.succeed);
    },

    function testIgnoreTouchScrolling() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      const touchScrollStartEvent = new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
      ]);
      stubElement.sendEvent(touchScrollStartEvent);
      chrome.test.assertEq(null, pinchListener.lastEvent);
      chrome.test.assertFalse(touchScrollStartEvent.defaultPrevented);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 1},
      ]));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testIgnoreWheelScrolling() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      // A wheel event where ctrlKey is false does not indicate zooming.
      const scrollingWheelEvent =
          new MockWheelEvent(1, {clientX: 0, clientY: 0}, false);
      stubElement.sendEvent(scrollingWheelEvent);
      chrome.test.assertEq(null, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testPreventNativePinchZoom() {
      const touchAction =
          window.getComputedStyle(document.documentElement).touchAction;

      chrome.test.assertEq('pan-x pan-y', touchAction);

      chrome.test.succeed();
    },

    function testPreventNativeZoomFromWheel() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      // Ensure that the wheel listener is not passive, otherwise the call to
      // preventDefault will be ignored. Since listeners could default to being
      // passive, we must set the value explicitly.
      for (const l of stubElement.listeners.get('wheel')) {
        const options = l.options;
        chrome.test.assertTrue(
            !!options && typeof (options.passive) == 'boolean');
        chrome.test.assertFalse(options.passive);
      }

      // We should not preventDefault a wheel event where ctrlKey is false as
      // that would prevent scrolling, not zooming.
      const scrollingWheelEvent =
          new MockWheelEvent(1, {clientX: 0, clientY: 0}, false);
      stubElement.sendEvent(scrollingWheelEvent);
      chrome.test.assertFalse(scrollingWheelEvent.defaultPrevented);

      const zoomingWheelEvent =
          new MockWheelEvent(1, {clientX: 0, clientY: 0}, true);
      stubElement.sendEvent(zoomingWheelEvent);
      chrome.test.assertTrue(zoomingWheelEvent.defaultPrevented);

      chrome.test.succeed();
    },

    function testWasTwoFingerTouch() {
      const stubElement = new StubElement();
      const gestureDetector = new GestureDetector(stubElement);


      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have two finger touch before first touch event.');

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
      ]));
      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have a two finger touch with one touch.');

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 2, clientY: 2},
      ]));
      chrome.test.assertTrue(
          gestureDetector.wasTwoFingerTouch(),
          'Should have a two finger touch.');

      // Make sure we keep |wasTwoFingerTouch| true after the end event.
      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertTrue(
          gestureDetector.wasTwoFingerTouch(),
          'Should maintain two finger touch after touchend.');

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 2, clientY: 2},
        {clientX: 4, clientY: 4},
      ]));
      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have two finger touch with 3 touches.');

      chrome.test.succeed();
    }
  ];
}());
