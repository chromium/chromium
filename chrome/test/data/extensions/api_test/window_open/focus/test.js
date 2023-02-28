// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectFocusChange;
var createdWinId;
var focusedWinId;
var listenDoneCallback;

function resetTest(focused) {
  expectFocusChange = focused;
  createdWinId = chrome.windows.WINDOW_ID_NONE;
  focusedWinId = chrome.windows.WINDOW_ID_NONE;
  listenDoneCallback = chrome.test.listenForever(
      chrome.windows.onFocusChanged, onFocusChanged);
}

function onFocusChanged(changedWinId) {
  if (chrome.windows.WINDOW_ID_NONE != changedWinId) {
    focusedWinId = changedWinId;
    if (expectFocusChange)
      maybeFocusedTestDone();
  }
}

function checkFocused(win) {
  createdWinId = win.id;
  maybeFocusedTestDone();
}

// Test is done when focus has changed to the created window.
function maybeFocusedTestDone() {
  if (focusedWinId != chrome.windows.WINDOW_ID_NONE &&
      createdWinId != chrome.windows.WINDOW_ID_NONE) {
    listenDoneCallback();
    chrome.test.assertEq(focusedWinId, createdWinId);
  }
}

function checkUnfocused(win) {
  createdWinId = win.id;
  setTimeout(chrome.test.callbackPass(function () {
      listenDoneCallback();
      chrome.test.assertNe(createdWinId, focusedWinId);
      }), 500);
}

chrome.test.runTests([
  function defaultHasFocus() {
    resetTest(true);
    chrome.windows.create(
        { 'url': 'blank.html' },
        chrome.test.callbackPass(checkFocused)
    );
  },
  function defaultHasFocusPopup() {
    resetTest(true);
    chrome.windows.create(
        { 'url': 'blank.html', 'type': 'popup' },
        chrome.test.callbackPass(checkFocused)
    );
  },
  function withFocus() {
    resetTest(true);
    chrome.windows.create(
        { 'url': 'blank.html', 'focused': true },
        chrome.test.callbackPass(checkFocused)
    );
  },
  function withFocusPopup() {
    resetTest(true);
    chrome.windows.create(
        { 'url': 'blank.html', 'focused': true, 'type': 'popup' },
        chrome.test.callbackPass(checkFocused)
    );
  },
  function withoutFocus() {
    resetTest(false);
    chrome.windows.create(
        { 'url': 'blank.html', 'focused': false },
        chrome.test.callbackPass(checkUnfocused)
    );
  },
  function withoutFocusPopup() {
    resetTest(false);
    chrome.windows.create(
        { 'url': 'blank.html', 'focused': false, 'type': 'popup' },
        chrome.test.callbackPass(checkUnfocused)
    );
  }
]);
