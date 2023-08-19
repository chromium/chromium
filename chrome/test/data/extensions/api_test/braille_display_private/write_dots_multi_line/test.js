// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.writeDotsMultiLine.
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
    chrome.test.assertTrue(state.available, 'Display not available');
    chrome.test.assertEq(20, state.textColumnCount);
    chrome.test.assertEq(7, state.textRowCount);
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
  function testWriteEmptyCells() {
    waitForDisplay(pass(function() {
      chrome.brailleDisplayPrivate.writeDots(new ArrayBuffer(0), 20, 7);
      chrome.brailleDisplayPrivate.writeDots(new ArrayBuffer(0), 19, 6);
      chrome.brailleDisplayPrivate.writeDots(new ArrayBuffer(0), 21, 8);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  },

  function testWriteOversizedCells() {
    waitForDisplay(pass(function(state) {
      chrome.brailleDisplayPrivate.writeDots(createBuffer(141, 1), 19, 9);
      chrome.brailleDisplayPrivate.writeDots(createBuffer(141, 2), 21, 8);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  },

  function testWriteUndersizedCellsNoCrash() {
    waitForDisplay(pass(function(state) {
      chrome.brailleDisplayPrivate.writeDots(createBuffer(100, 3), 10, 2);
      chrome.brailleDisplayPrivate.getDisplayState(pass());
    }));
  }
]);
