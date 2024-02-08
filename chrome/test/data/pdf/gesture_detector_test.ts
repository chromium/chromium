// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PinchEventDetail} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {GestureDetector} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {createWheelEvent} from './test_util.js';

chrome.test.runTests(function() {
  'use strict';

  class PinchListener {
    lastEvent: CustomEvent<PinchEventDetail>|null = null;

    constructor(gestureDetector: GestureDetector) {
      gestureDetector.getEventTarget().addEventListener(
          'pinchstart', e => this.onPinch_(e as CustomEvent<PinchEventDetail>));
      gestureDetector.getEventTarget().addEventListener(
          'pinchupdate',
          e => this.onPinch_(e as CustomEvent<PinchEventDetail>));
      gestureDetector.getEventTarget().addEventListener(
          'pinchend', e => this.onPinch_(e as CustomEvent<PinchEventDetail>));
    }

    private onPinch_(pinchEvent: CustomEvent<PinchEventDetail>) {
      this.lastEvent = pinchEvent;
    }
  }

  let stubElement: HTMLElement;

  function createStubElement(): HTMLElement {
    const stubElement = document.createElement('div');
    document.body.innerHTML = '';
    document.body.appendChild(stubElement);
    return stubElement;
  }

  function createTouchEvent(
      type: string, touches: Array<Partial<TouchInit>>): TouchEvent {
    return new TouchEvent(type, {
      touches: touches.map(t => {
        return new Touch(
            Object.assign({identifier: 0, target: stubElement}, t));
      }),
      // Necessary for preventDefault() to work.
      cancelable: true,
    });
  }

  return [
    function testTransformCenter() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      stubElement.style.position = 'absolute';
      stubElement.style.left = '1px';
      stubElement.style.top = '-1px';
      stubElement.dispatchEvent(
          createWheelEvent(1, {clientX: 2, clientY: 3}, true));
      chrome.test.assertEq('pinchupdate', pinchListener.lastEvent!.type);
      chrome.test.assertEq(
          {x: 1, y: 4}, pinchListener.lastEvent!.detail.center);

      chrome.test.succeed();
    },

    function testPinchZoomIn() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      stubElement.dispatchEvent(createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      chrome.test.assertEq('pinchstart', pinchListener.lastEvent!.type);
      chrome.test.assertEq(
          {center: {x: 0, y: 1}}, pinchListener.lastEvent!.detail);

      stubElement.dispatchEvent(createTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4},
      ]));
      chrome.test.assertEq('pinchupdate', pinchListener.lastEvent!.type);
      chrome.test.assertEq(
          {
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 2,
            center: {x: 0, y: 2},
          },
          pinchListener.lastEvent!.detail);

      stubElement.dispatchEvent(createTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 8},
      ]));
      chrome.test.assertEq('pinchupdate', pinchListener.lastEvent!.type);
      chrome.test.assertEq(
          {
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 4,
            center: {x: 0, y: 4},
          },
          pinchListener.lastEvent!.detail);

      stubElement.dispatchEvent(createTouchEvent('touchend', []));
      chrome.test.assertEq('pinchend', pinchListener.lastEvent!.type);
      chrome.test.assertEq(
          {startScaleRatio: 4, center: {x: 0, y: 4}},
          pinchListener.lastEvent!.detail);

      chrome.test.succeed();
    },

    function testPinchZoomInAndBackOut() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      stubElement.dispatchEvent(createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      let {type, detail} = pinchListener.lastEvent!;
      chrome.test.assertEq('pinchstart', type);
      chrome.test.assertEq({center: {x: 0, y: 1}}, detail);

      stubElement.dispatchEvent(createTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4},
      ]));
      ({type, detail} = pinchListener.lastEvent!);
      chrome.test.assertEq('pinchupdate', type);
      chrome.test.assertEq(
          {
            scaleRatio: 2,
            direction: 'in',
            startScaleRatio: 2,
            center: {x: 0, y: 2},
          },
          detail);

      stubElement.dispatchEvent(createTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2},
      ]));
      // This should be part of the same gesture as an update.
      // A change in direction should not end the gesture and start a new one.
      ({type, detail} = pinchListener.lastEvent!);
      chrome.test.assertEq('pinchupdate', type);
      chrome.test.assertEq(
          {
            scaleRatio: 0.5,
            direction: 'out',
            startScaleRatio: 1,
            center: {x: 0, y: 1},
          },
          detail);

      stubElement.dispatchEvent(createTouchEvent('touchend', []));
      ({type, detail} = pinchListener.lastEvent!);
      chrome.test.assertEq('pinchend', type);
      chrome.test.assertEq({startScaleRatio: 1, center: {x: 0, y: 1}}, detail);

      chrome.test.succeed();
    },

    async function testZoomWithWheel() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      // Since the wheel events that the GestureDetector receives are
      // individual updates without begin/end events, we need to make sure the
      // GestureDetector generates appropriate pinch begin/end events itself.
      class PinchSequenceListener {
        seenBegin: boolean = false;
        endPromise: Promise<void>;

        constructor(gestureDetector: GestureDetector) {
          gestureDetector.getEventTarget().addEventListener(
              'pinchstart', () => {
                this.seenBegin = true;
              });

          this.endPromise = new Promise<void>(function(resolve) {
            gestureDetector.getEventTarget().addEventListener(
                'pinchend', () => resolve());
          });
        }
      }
      const pinchSequenceListener = new PinchSequenceListener(gestureDetector);

      const scale = 1.23;
      const deltaY = -(100.0 * Math.log(scale));
      const position = {clientX: 12, clientY: 34};
      stubElement.dispatchEvent(createWheelEvent(deltaY, position, true));

      chrome.test.assertTrue(pinchSequenceListener.seenBegin);

      const {type, detail} = pinchListener.lastEvent!;
      chrome.test.assertEq('pinchupdate', type);
      chrome.test.assertTrue(detail.scaleRatio !== null);
      chrome.test.assertTrue(Math.abs(detail.scaleRatio! - scale) < 0.001);
      chrome.test.assertEq('in', detail.direction);
      chrome.test.assertTrue(detail.startScaleRatio !== null);
      chrome.test.assertTrue(Math.abs(detail.startScaleRatio! - scale) < 0.001);
      chrome.test.assertEq(
          {x: position.clientX, y: position.clientY}, detail.center);

      await pinchSequenceListener.endPromise;

      chrome.test.succeed();
    },

    function testIgnoreTouchScrolling() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      const touchScrollStartEvent = createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
      ]);
      stubElement.dispatchEvent(touchScrollStartEvent);
      chrome.test.assertEq(null, pinchListener.lastEvent);
      chrome.test.assertFalse(touchScrollStartEvent.defaultPrevented);

      stubElement.dispatchEvent(createTouchEvent('touchmove', [
        {clientX: 0, clientY: 1},
      ]));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      stubElement.dispatchEvent(createTouchEvent('touchend', []));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testIgnoreWheelScrolling() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      const pinchListener = new PinchListener(gestureDetector);

      // A wheel event where ctrlKey is false does not indicate zooming.
      stubElement.dispatchEvent(
          createWheelEvent(1, {clientX: 0, clientY: 0}, false));
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
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);
      new PinchListener(gestureDetector);

      // We should not preventDefault a wheel event where ctrlKey is false as
      // that would prevent scrolling, not zooming.
      const scrollingWheelEvent =
          createWheelEvent(1, {clientX: 0, clientY: 0}, false);
      stubElement.dispatchEvent(scrollingWheelEvent);
      chrome.test.assertFalse(scrollingWheelEvent.defaultPrevented);

      const zoomingWheelEvent =
          createWheelEvent(1, {clientX: 0, clientY: 0}, true);
      stubElement.dispatchEvent(zoomingWheelEvent);
      chrome.test.assertTrue(zoomingWheelEvent.defaultPrevented);

      chrome.test.succeed();
    },

    function testWasTwoFingerTouch() {
      stubElement = createStubElement();
      const gestureDetector = new GestureDetector(stubElement);


      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have two finger touch before first touch event.');

      stubElement.dispatchEvent(createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
      ]));
      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have a two finger touch with one touch.');

      stubElement.dispatchEvent(createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 2, clientY: 2},
      ]));
      chrome.test.assertTrue(
          gestureDetector.wasTwoFingerTouch(),
          'Should have a two finger touch.');

      // Make sure we keep |wasTwoFingerTouch| true after the end event.
      stubElement.dispatchEvent(createTouchEvent('touchend', []));
      chrome.test.assertTrue(
          gestureDetector.wasTwoFingerTouch(),
          'Should maintain two finger touch after touchend.');

      stubElement.dispatchEvent(createTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 2, clientY: 2},
        {clientX: 4, clientY: 4},
      ]));
      chrome.test.assertFalse(
          gestureDetector.wasTwoFingerTouch(),
          'Should not have two finger touch with 3 touches.');

      chrome.test.succeed();
    },
  ];
}());
