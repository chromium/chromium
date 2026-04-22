// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.onKeyEvent.
// browser_tests.exe --gtest_filter='BrailleDisplayPrivateApiTest.*'

const pass = chrome.test.callbackPass;

const EXPECTED_EVENTS = [
  {command: 'line_up'},
  {command: 'line_down'},
  {command: 'pan_left'},
  {command: 'pan_right'},
  {command: 'top'},
  {command: 'bottom'},
  {command: 'routing', displayPosition: 5},
  {command: 'standard_key', standardKeyChar: 'A'},
  {command: 'standard_key', standardKeyChar: '\u00E5'},
  {command: 'standard_key', standardKeyChar: '\u0100'},
  // UTF-16 of U+1F639.
  {command: 'standard_key', standardKeyChar: '\uD83D\uDE39'},
  {command: 'standard_key', standardKeyCode: 'Backspace'},
  {command: 'standard_key', standardKeyCode: 'Tab', shiftKey: true},
  {command: 'standard_key', standardKeyCode: 'F3', altKey: true},
  {command: 'dots', ctrlKey: true, brailleDots: 0x1 | 0x2},
];
for (let i = 0; i < 256; ++i) {
  EXPECTED_EVENTS.push({command: 'dots', brailleDots: i});
}

let eventNumber = 0;
let allEventsReceived;

function eventListener(event) {
  chrome.test.assertTrue(eventNumber < EXPECTED_EVENTS.length);
  chrome.test.assertEq(EXPECTED_EVENTS[eventNumber], event);
  if (++eventNumber == EXPECTED_EVENTS.length) {
    allEventsReceived();
  }
}

function waitForDisplay(callback) {
  let callbackCompleted = chrome.test.callbackAdded();
  const displayStateHandler = function(state) {
    if (!callbackCompleted) {
      return;
    }
    chrome.test.assertTrue(state.available, 'Display not available');
    chrome.test.assertEq(11, state.textColumnCount);
    callback(state);
    callbackCompleted();
    chrome.brailleDisplayPrivate.onDisplayStateChanged.removeListener(
        displayStateHandler);
    // Prevent additional runs if the onDisplayStateChanged event
    // is fired before getDisplayState invokes the callback.
    callbackCompleted = null;
  };
  chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
      displayStateHandler);
  chrome.brailleDisplayPrivate.getDisplayState(pass(function(state) {
    if (state.available) {
      displayStateHandler(state);
    } else {
      console.log('Display not ready yet');
    }
  }));
}

chrome.test.runTests([
  function testKeyEvents() {
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(eventListener);
    allEventsReceived = chrome.test.callbackAdded();
    waitForDisplay(pass());
  },
]);
