// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var previousTop = 0;
var left = 0;
var width = 0;
var height = 0;

var chromeWindow = null;

function checkDimensions(currentWindow) {
  chrome.test.assertEq(previousTop, currentWindow.top);
  chrome.test.assertEq(left, currentWindow.left);
  chrome.test.assertEq(width, currentWindow.width);
  chrome.test.assertEq(height, currentWindow.height);
}

function setFocus(tab) {
  previousTop = chromeWindow.top;
  left = chromeWindow.left;
  width = chromeWindow.width;
  height = chromeWindow.height;

  chrome.windows.update(
    chromeWindow.id, { 'focused': true },
    pass(checkDimensions)
  );
}

chrome.test.runTests([
  function setFocusWithNoResize() {
    chrome.windows.getCurrent(
      pass(function(currentWindow) {
        chromeWindow = currentWindow;
        chrome.tabs.create(
          { 'windowId': currentWindow.id, 'url': 'blank.html' },
          pass(setFocus)
        );
    }));
  }
]);
