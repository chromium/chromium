// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), when current window is null
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureNullWindow

chrome.test.runTests([function captureNullWindow() {
  // Create a new window so we don't close the only active window.
  chrome.windows.create(function(newWindow) {
    chrome.windows.remove(newWindow.id, function() {
      chrome.tabs.captureVisibleTab(
          newWindow.id, function() {
        // The error message is non-deterministic based on how far we've gone
        // in removing the window.
        const error1 = `No window with id: ${newWindow.id}.`;
        const error2 = 'No active web contents to capture';
        chrome.test.assertTrue(!!chrome.runtime.lastError);
        let actualError = chrome.runtime.lastError.message;
        chrome.test.assertTrue(actualError == error1 || actualError == error2,
                               actualError);
        chrome.test.succeed();
      });
    });
  });
}]);
