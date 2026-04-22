// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;

const finalTop = 400;
const finalLeft = 10;
const finalWidth = 640;
const finalHeight = 401;

let chromeWindow = null;

function checkTop(currentWindow) {
  chrome.test.assertEq(finalTop, currentWindow.top);
}

function checkHeightAndContinue(currentWindow) {
  chrome.test.assertEq(finalHeight, currentWindow.height);
  chrome.windows.update(
      currentWindow.id,
      {'top': finalTop},
      pass(checkTop),
  );
}

function checkWidthAndContinue(currentWindow) {
  chrome.test.assertEq(finalWidth, currentWindow.width);
  chrome.windows.update(
      currentWindow.id,
      {'height': finalHeight},
      pass(checkHeightAndContinue),
  );
}

function checkLeftAndContinue(currentWindow) {
  chrome.test.assertEq(finalLeft, currentWindow.left);
  chrome.windows.update(
      currentWindow.id,
      {'width': finalWidth},
      pass(checkWidthAndContinue),
  );
}

function updateLeftAndContinue(tab) {
  chrome.windows.update(
      chromeWindow.id,
      {'left': finalLeft},
      pass(checkLeftAndContinue),
  );
}

chrome.test.runTests([
  function setResizeWindow() {
    chrome.windows.getCurrent(pass(function(currentWindow) {
      chromeWindow = currentWindow;
      chrome.tabs.create(
          {'windowId': currentWindow.id, 'url': 'blank.html'},
          pass(updateLeftAndContinue),
      );
    }));
  },
]);
