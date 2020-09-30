// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var createWindowUtil = function(urlToLoad, createdCallback) {
  try {
    chrome.windows.create({ 'url': urlToLoad, 'type': 'normal',
        'width': 600, 'height': 400 }, function(window) {
      createdCallback({width: window.width, height: window.height});
    });
  } catch (e) {
    chrome.test.fail(e);
  }
}

var getAllWindowUtil = function(populateValue, getAllCallback) {
  try {
    chrome.windows.getAll({populate: populateValue}, function(window) {
      getAllCallback({length: window.length});
    });
  } catch (e) {
    chrome.test.fail(e);
  }
}

chrome.test.runTests([
  // Get the window that was automatically created.
  function testWindowGetBeforeCreate() {
    var populateValue = true;
    getAllWindowUtil(populateValue, function(windowData) {
      chrome.test.assertEq(1, windowData.length);
      chrome.test.succeed();
    });
  },
  // Create a new window.
  function testWindowCreate() {
    createWindowUtil('blank.html', function(windowData) {
      chrome.test.assertEq(600, windowData.width);
      chrome.test.assertEq(400, windowData.height);
      chrome.test.succeed();
    });
  },
  // Check that the created window exists.
  function testWindowGetAfterCreate() {
    var populateValue = true;
    getAllWindowUtil(populateValue, function(windowData) {
      chrome.test.assertEq(2, windowData.length);
      chrome.test.succeed();
    });
  },
]);
