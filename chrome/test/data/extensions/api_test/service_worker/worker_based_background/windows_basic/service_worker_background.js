// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const createWindowUtil = function(urlToLoad, createdCallback) {
  try {
    chrome.windows.create(
        {url: urlToLoad, type: 'normal', width: 600, height: 500},
        createdCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
};

const getAllWindowUtil = function(populateValue, getAllCallback) {
  try {
    chrome.windows.getAll({populate: populateValue}, getAllCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
};

const getWindowUtil = function(windowId, getCallback) {
  try {
    chrome.windows.get(windowId, getCallback);
  } catch (e) {
    chrome.test.fail(e);
  }
};

chrome.test.getConfig((config) => {
  // On Android, requested window bounds are only applied when the window can be
  // resized, and may differ slightly due to rounding during dip->px
  // conversion. The C++ side passes 'skipBoundsChecks' or
  // 'allowBoundsTolerance' accordingly.
  const checkBounds = config.customArg !== 'skipBoundsChecks';
  const boundsToleranceDip =
      config.customArg === 'allowBoundsTolerance' ? 2 : 0;
  const assertSizeNear = function(expected, actual) {
    chrome.test.assertTrue(
        Math.abs(actual - expected) <= boundsToleranceDip,
        `Expected ${expected} (+/- ${boundsToleranceDip}), got ${actual}`);
  };

  chrome.test.runTests([
    // Get the window that was automatically created.
    function testWindowGetAllBeforeCreate() {
      const populateValue = true;
      getAllWindowUtil(populateValue, function(allWindowsData) {
        chrome.test.assertEq(1, allWindowsData.length);
        chrome.test.succeed();
      });
    },
    // Create a new window.
    function testWindowCreate() {
      createWindowUtil('blank.html', function(createdWindowData) {
        if (checkBounds) {
          assertSizeNear(600, createdWindowData.width);
          assertSizeNear(500, createdWindowData.height);
        }
        chrome.test.succeed();
      });
    },
    // Check that the created window exists.
    function testWindowGetAllAfterCreate() {
      const populateValue = true;
      getAllWindowUtil(populateValue, function(allWindowsData) {
        chrome.test.assertEq(2, allWindowsData.length);
        const createdWindowId = allWindowsData[allWindowsData.length - 1].id;
        getWindowUtil(createdWindowId, function(windowData) {
          if (checkBounds) {
            assertSizeNear(600, windowData.width);
            assertSizeNear(500, windowData.height);
          }
          chrome.test.succeed();
        });
      });
    },
  ]);
});
