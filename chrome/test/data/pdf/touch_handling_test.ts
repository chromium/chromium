// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const viewer = document.body.querySelector('pdf-viewer')!;

function sendTouchStart(touches: Point[]) {
  let id = 0;
  const touchList = touches.map(function(xy: Point) {
    const touchInit = {
      identifier: id++,
      target: viewer.shadowRoot!.querySelector('embed')!,
      clientX: xy.x,
      clientY: xy.y,
    };

    return new window.Touch(touchInit);
  });

  const target = viewer.shadowRoot!.querySelector('#content')!;
  target.dispatchEvent(new TouchEvent('touchstart', {
    bubbles: true,
    composed: true,
    touches: touchList,
    targetTouches: touchList,
    changedTouches: touchList,
  }));
}

function createContextMenuEvent() {
  return new MouseEvent('contextmenu', {
    bubbles: true,
    composed: true,
    cancelable: true,
    sourceCapabilities: new InputDeviceCapabilities({firesTouchEvents: true}),
  });
}

chrome.test.runTests([
  // Test suppression of the context menu on single touch.
  function testContextMenuSingleTouch() {
    sendTouchStart([{x: 10, y: 10}]);

    const event = createContextMenuEvent();
    // Dispatch event will be false if the event is cancellable and one of the
    // handlers called preventDefault.
    chrome.test.assertFalse(
        document.dispatchEvent(event),
        'Should have called preventDefault() for single touch.');
    chrome.test.succeed();
  },

  // Test allowing of context menu on double touch.
  function testContextMenuDoubleTouch() {
    sendTouchStart([{x: 10, y: 10}, {x: 15, y: 15}]);

    const event = createContextMenuEvent();
    chrome.test.assertTrue(
        document.dispatchEvent(event),
        'Should not have called preventDefault() for double touch.');
    chrome.test.succeed();
  },

  // Test long press selects word. This test flakes out on some bots.
  // The test passes locally on MacOS, ChromeOS and Linux. Disable until it's
  // possible to repro the bot issue. https://crbug.com/723632
  // function testLongPressSelectsText() {
  //   var client = new PdfScriptingApi(window, window);
  //   sendTouchStart([{x: 336, y: 163}]);
  //   window.setTimeout(function() {
  //     client.getSelectedText(
  //       chrome.test.callbackPass(function(selectedText) {
  //         chrome.test.assertEq('some', selectedText);
  //       })
  //     );
  //     chrome.test.succeed();
  //   // 10k is the value for the action_timeout_ms_ in Chrome test_timeouts.cc
  //   }, 10000);
  // }
]);
