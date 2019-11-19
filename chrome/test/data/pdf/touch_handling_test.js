// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';

function sendTouchStart(touches) {
  let id = 0;
  const touchList = touches.map(function(xy) {
    const touchInit = {
      identifier: id++,
      target: viewer.plugin_,
      clientX: xy.x,
      clientY: xy.y,
    };

    return new window.Touch(touchInit);
  });

  const target = document.getElementById('content');
  target.dispatchEvent(new TouchEvent('touchstart', {
    touches: touchList,
    targetTouches: touchList,
    changedtouches: touchList
  }));
}

function createContextMenuEvent() {
  return new MouseEvent('contextmenu', {
    cancelable: true,
    sourceCapabilities: new InputDeviceCapabilities({firesTouchEvents: true})
  });
}

const tests = [
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
  //   var client = new PDFScriptingAPI(window, window);
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
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
