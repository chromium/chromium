// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var createWindowUtil = function(urlToLoad, createdCallback) {
  try {
    chrome.windows.create({ 'url': urlToLoad, 'type': 'normal',
        'width': 600, 'height': 400 }, createdCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
}

var getAllWindowUtil = function(populateValue, getAllCallback) {
  try {
    chrome.windows.getAll({populate: populateValue}, getAllCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
}

var getWindowUtil = function(windowId, getCallback) {
  try {
    chrome.windows.get(windowId, getCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
}

chrome.test.runTests([
  // Get the window that was automatically created.
  function testWindowGetAllBeforeCreate() {
    var populateValue = true;
    getAllWindowUtil(populateValue, function(allWindowsData) {
      chrome.test.assertEq(1, allWindowsData.length);
      chrome.test.succeed();
    });
  },
  // Create a new window.
  function testWindowCreate() {
    createWindowUtil('blank.html', function(createdWindowData) {
      chrome.test.assertEq(600, createdWindowData.width);
      chrome.test.assertEq(400, createdWindowData.height);
      chrome.test.succeed();
    });
  },
  // Check that the created window exists.
  function testWindowGetAllAfterCreate() {
    var populateValue = true;
    getAllWindowUtil(populateValue, function(allWindowsData) {
      chrome.test.assertEq(2, allWindowsData.length);
      var createdWindowId = allWindowsData[allWindowsData.length - 1].id;
      getWindowUtil(createdWindowId, function(windowData) {
        chrome.test.assertEq(600, windowData.width);
        chrome.test.assertEq(400, windowData.height);
        chrome.test.succeed();
      });
    });
  },
  ]);
