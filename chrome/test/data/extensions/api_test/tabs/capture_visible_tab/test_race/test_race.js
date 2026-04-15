// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// API test for chrome.tabs.captureVisibleTab() that tests that concurrent
// capture requests end up with the expected data (by opening 8 windows with
// alternating black and white contents and asserting that the captured pixels
// are of the expected colors).
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabRace

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const scriptUrl =
    '_test_resources/api_test/tabs/capture_visible_tab/common/tabs_util.js';

const kWindowRect = {
  width: 400,
  height: 400,
};

function log(message) {
  console.log(`${new Date().toLocaleTimeString()} - ${message}`);
}

const loadScript = chrome.test.loadScript(scriptUrl);
loadScript.then(() => {chrome.test.runTests([

  function captureVisibleTabRace() {
    // Simulate a callback being added to make sure that the test isn't
    // considered complete until all of the 8 windows are tested (this happens
    // in parallel, so the normal nesting of pass() calls is not sufficient).
    const callbackCompleted = chrome.test.callbackAdded();

    log('creating windows');

    const windowsAndColors = [];
    for (let i = 0; i < 8; i++) {
      let colorName;
      let expectedColor;
      if (i % 2) {
        colorName = 'white';
        expectedColor = '255,255,255,255';
      } else {
        colorName = 'black';
        expectedColor = '0,0,0,255';
      }
      const url = chrome.runtime.getURL(`/common/${colorName}.html`);
      createWindow(
          [url],
          kWindowRect,
          (function(expectedColor, winId, tabIds) {
            log(`created ${winId}`);
            windowsAndColors.push([winId, expectedColor]);
          }).bind(this, expectedColor));
    }

    waitForAllTabs(function() {
      log('capturing contents');

      let testedWindowCount = 0;
      windowsAndColors.forEach(function(windowIdAndExpectedColor) {
        const windowId = windowIdAndExpectedColor[0];
        const expectedColor = windowIdAndExpectedColor[1];
        chrome.tabs.captureVisibleTab(
            windowId,
            {format: 'png'},
            function(imgDataUrl) {
              log(`captured ${windowId}`);
              if (chrome.runtime.lastError) {
                chrome.test.fail('captureVisibleTab error: ' +
                    chrome.runtime.lastError.message);
              }
              testPixelsAreExpectedColor(
                  imgDataUrl, kWindowRect, expectedColor);
              log(`tested pixels for ${windowId}`);
              testedWindowCount++;
              if (testedWindowCount == windowsAndColors.length) {
                log('test complete');
                callbackCompleted();
              }
            });
      });
    });
 }
])});
