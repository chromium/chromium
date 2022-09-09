// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var widthDelta = 10;
var heightDelta = 20;

var expectedWidth;
var expectedHeight;

function finishTest(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  chrome.windows.remove(currentWindow.id, pass());
}

function changeWidthAndHeight(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  expectedWidth = currentWindow.width + widthDelta;
  expectedHeight = currentWindow.height + heightDelta;
  chrome.windows.update(
    currentWindow.id, { 'width': expectedWidth , 'height': expectedHeight},
    pass(finishTest)
  );
}

function changeHeight(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  expectedWidth = currentWindow.width;
  expectedHeight = currentWindow.height + heightDelta;
  chrome.windows.update(
    currentWindow.id, { 'height': expectedHeight },
    pass(changeWidthAndHeight)
  );
}

function changeWidth(currentWindow) {
  expectedWidth = currentWindow.width + widthDelta;
  expectedHeight = currentWindow.height;
  chrome.windows.update(
    currentWindow.id, { 'width': expectedWidth },
    pass(changeHeight)
  );
}

chrome.test.runTests([
  // Tests windows.update use of the chrome.windows.WINDOW_ID_CURRENT constant.
  function testCurrentWindowResize() {
    var newWidth = 500;
    chrome.windows.create(
        { 'url': 'blank.html', 'top': 0, 'left': 0, 'width': 500, 'height': 400,
          'type': 'normal' },
        pass(function(win1) {
      chrome.windows.getCurrent(pass(function(win2) {
        chrome.test.assertEq(win1.id, win2.id);
        chrome.windows.update(
            chrome.windows.WINDOW_ID_CURRENT, { 'width': newWidth },
            pass(function(win3) {
          chrome.test.assertEq(win2.id, win3.id);
          chrome.test.assertEq(newWidth, win3.width);
          chrome.test.assertEq(win2.height, win3.height);
        }));
      }));
    }));
  },

  function testResizeNormal() {
    chrome.windows.create(
        { 'url': 'blank.html', 'top': 0, 'left': 0, 'width': 500, 'height': 500,
          'type': 'normal' },
        pass(changeWidth));
  },
  function testResizePopup() {
    chrome.windows.create(
        { 'url': 'blank.html', 'top': 0, 'left': 0, 'width': 300, 'height': 400,
          'type': 'popup' },
        pass(changeWidth));
  },
  function testResizePanel() {
    chrome.windows.create(
        { 'url': 'blank.html', 'top': 0, 'left': 0, 'width': 150, 'height': 200,
          'type': 'panel' },
        pass(changeWidth));
  },
]);
