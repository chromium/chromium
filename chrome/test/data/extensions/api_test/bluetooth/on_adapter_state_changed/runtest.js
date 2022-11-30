// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testEvents() {
  chrome.test.assertEq(kExpectedValues.length, states.length);

  for (var i = 0; i < kExpectedValues.length; ++i) {
    chrome.test.assertEq(kExpectedValues[i], states[i].powered);
    chrome.test.assertEq(kExpectedValues[i], states[i].available);
    chrome.test.assertEq(kExpectedValues[i], states[i].discovering);
  }

  chrome.test.succeed();
}

var states = [];
var kExpectedValues = [false, true, true];
chrome.bluetooth.onAdapterStateChanged.addListener(
    function(state) {
      states.push(state);
    });
chrome.test.sendMessage('ready',
    function(message) {
      chrome.test.runTests([
          testEvents
      ]);
    });
