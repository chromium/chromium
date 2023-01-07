// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SwipeDetector, SwipeDirection} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

chrome.test.runTests(function() {
  'use strict';

  class SwipeListener {
    lastEvent: CustomEvent<SwipeDirection>|null = null;
    constructor(swipeDetector: SwipeDetector) {
      swipeDetector.getEventTarget().addEventListener(
          'swipe', e => this.onSwipe_(e as CustomEvent<SwipeDirection>));

      // Make sure the swipe is fast enough to be considered as a swipe action.
      swipeDetector.setElapsedTimerForTesting(10);
    }

    private onSwipe_(swipeEvent: CustomEvent<SwipeDirection>) {
      this.lastEvent = swipeEvent;
    }
  }

  let stubElement: HTMLElement;

  function createStubElement(): HTMLElement {
    const stubElement = document.createElement('div');
    document.body.innerHTML = '';
    document.body.appendChild(stubElement);
    return stubElement;
  }

  function createTouchEventForSwipes(
      type: string, touches: Array<Partial<TouchInit>>,
      changedTouches: Array<Partial<TouchInit>>): TouchEvent {
    return new TouchEvent(type, {
      touches: touches.map(t => {
        return new Touch(
            Object.assign({identifier: 0, target: stubElement}, t));
      }),
      changedTouches: changedTouches.map(t => {
        return new Touch(
            Object.assign({identifier: 0, target: stubElement}, t));
      }),
      // Necessary for preventDefault() to work.
      cancelable: true,
    });
  }

  return [
    function testSwipeRightInPresentationMode() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      swipeDetector.setPresentationMode(true);
      chrome.test.assertTrue(swipeDetector.getPresentationModeForTesting());

      // Swipe from left to right.
      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 0, clientY: 0}], [{pageX: 0, pageY: 0}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      stubElement.dispatchEvent(
          createTouchEventForSwipes('touchend', [], [{pageX: 200, pageY: 0}]));
      chrome.test.assertTrue(!!swipeListener.lastEvent);
      chrome.test.assertEq(
          SwipeDirection.LEFT_TO_RIGHT, swipeListener.lastEvent.detail);

      chrome.test.succeed();
    },

    function testSwipeLeftInPresentationMode() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      swipeDetector.setPresentationMode(true);
      chrome.test.assertTrue(swipeDetector.getPresentationModeForTesting());

      // Swipe from right to left.
      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 300, clientY: 0}],
          [{pageX: 300, pageY: 0}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      stubElement.dispatchEvent(
          createTouchEventForSwipes('touchend', [], [{pageX: 100, pageY: 0}]));
      chrome.test.assertTrue(!!swipeListener.lastEvent);
      chrome.test.assertEq(
          SwipeDirection.RIGHT_TO_LEFT, swipeListener.lastEvent.detail);

      chrome.test.succeed();
    },

    function testSwipeInNonPresentationMode() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      chrome.test.assertFalse(swipeDetector.getPresentationModeForTesting());

      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 0, clientY: 0}], [{pageX: 0, pageY: 0}]));
      stubElement.dispatchEvent(
          createTouchEventForSwipes('touchend', [], [{pageX: 200, pageY: 0}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      chrome.test.succeed();
    },

    function testSwipeWithXDistTooSmall() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      swipeDetector.setPresentationMode(true);
      chrome.test.assertTrue(swipeDetector.getPresentationModeForTesting());

      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 0, clientY: 0}], [{pageX: 0, pageY: 0}]));
      stubElement.dispatchEvent(
          createTouchEventForSwipes('touchend', [], [{pageX: 149, pageY: 0}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      chrome.test.succeed();
    },

    function testSwipeWithYDistanceTooLarge() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      swipeDetector.setPresentationMode(true);
      chrome.test.assertTrue(swipeDetector.getPresentationModeForTesting());

      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 0, clientY: 0}], [{pageX: 0, pageY: 0}]));
      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchend', [], [{pageX: 200, pageY: 101}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      chrome.test.succeed();
    },

    function testSwipeTooSlow() {
      stubElement = createStubElement();
      const swipeDetector = new SwipeDetector(stubElement);
      const swipeListener = new SwipeListener(swipeDetector);

      swipeDetector.setPresentationMode(true);
      chrome.test.assertTrue(swipeDetector.getPresentationModeForTesting());

      // Make sure this swipe takes longer than |SWIPE_TIMER_INTERVAL_MS|.
      swipeDetector.setElapsedTimerForTesting(201);

      stubElement.dispatchEvent(createTouchEventForSwipes(
          'touchstart', [{clientX: 0, clientY: 0}], [{pageX: 0, pageY: 0}]));
      stubElement.dispatchEvent(
          createTouchEventForSwipes('touchend', [], [{pageX: 200, pageY: 0}]));
      chrome.test.assertEq(null, swipeListener.lastEvent);

      chrome.test.succeed();
    },
  ];
}());
