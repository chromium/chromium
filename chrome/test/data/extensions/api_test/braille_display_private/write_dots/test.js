// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.writeDots.
// browser_tests.exe --gtest_filter="BrailleDisplayPrivateApiTest.*"

var pass = chrome.test.callbackPass;

function createBuffer(size, element) {
  var buf = new Uint8Array(size);
  for (var i = 0; i < size; ++i) {
    buf[i] = element;
  }
  return buf.buffer;
}

function waitForDisplay(callback) {
  var callbackCompleted = chrome.test.callbackAdded();
  var displayStateHandler = function(state) {
    if (!callbackCompleted) {
      return;
    }
    chrome.test.assertTrue(state.available, "Display not available");
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
      console.log("Display not ready yet");
    }
  }));
}

chrome.test.runTests([
  function testWriteEmptyCells() {
    waitForDisplay(pass(function() {
      chrome.brailleDisplayPrivate.writeDots(new ArrayBuffer(0), 0, 0);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  },

  function testWriteOversizedCells() {
    waitForDisplay(pass(function(state) {
      chrome.brailleDisplayPrivate.writeDots(
          createBuffer(state.textColumnCount + 1, 1), state.textColumnCount, 1);
      chrome.brailleDisplayPrivate.writeDots(
          createBuffer(1000000, 2), 1000000, 1);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  },

  function testWriteUndersizedCellsNoCrash() {
    waitForDisplay(pass(function(state) {
      chrome.brailleDisplayPrivate.writeDots(
          createBuffer(state.textColumnCount - 2, 3), state.textColumnCount, 1);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  }
]);
