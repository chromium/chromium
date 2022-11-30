// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.OnDisplayStateChanged events.
// browser_tests.exe --gtest_filter="BrailleDisplayPrivateApiTest.*"

var pass = chrome.test.callbackPass;

var callbackCompleted;
var EXPECTED_EVENTS = [
  {'available': true, 'textColumnCount': 11, 'textRowCount': 1, cellSize: 6},
  {'available': false},
  {'available': true, 'textColumnCount': 22, 'textRowCount': 1, cellSize: 6},
];

var eventNumber = 0;

function eventListener(event) {
  console.log("Got event " + JSON.stringify(event));
  chrome.test.assertEq(event, EXPECTED_EVENTS[eventNumber]);
  if (++eventNumber == EXPECTED_EVENTS.length) {
    callbackCompleted();
  }
}

chrome.test.runTests([
  function testStateChanges() {
    chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
        eventListener);
    callbackCompleted = chrome.test.callbackAdded();
  }
]);
