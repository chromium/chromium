// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), capturing PNG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabPng

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWindowRect = {
  'width': 400,
  'height': 400
};

var kTestDir = '/extensions/api_test/tabs/capture_visible_tab/test_png/';
var kURLBaseA = 'http://a.com:PORT' + kTestDir;

var whiteImageUrl;
var textImageUrl;

var scriptUrl =
    '_test_resources/api_test/tabs/capture_visible_tab/common/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);
loadScript.then(() => {chrome.test.getConfig(function(config) {
  var fixPort = function(url) {
    return url.replace(/PORT/, config.testServer.port);
  };

  chrome.test.runTests([
    // Open a window with one tab, take a snapshot.
    function captureVisibleTabWhiteImage() {
      // Keep the resulting image small by making the window small.
      createWindow([fixPort(kURLBaseA + 'white.html')],
                   kWindowRect,
                   pass(function(winId, tabIds) {
        waitForAllTabs(pass(function() {
          chrome.tabs.query({active: true, windowId: winId},
                            pass(function(tabs) {
            // waitForAllTabs ensures this.
            assertEq('complete', tabs[0].status);
            chrome.tabs.captureVisibleTab(winId,
                                          {'format': 'png'},
                                          pass(function(imgDataUrl) {
              // The URL should be a data URL with has a PNG mime type.
              assertIsStringWithPrefix('data:image/png;base64,', imgDataUrl);
              whiteImageUrl = imgDataUrl;

              testPixelsAreExpectedColor(whiteImageUrl,
                                         kWindowRect,
                                         '255,255,255,255');  // White.
            }));
          }));
        }));
      }));
    },

    function captureVisibleTabText() {
      // Keep the resulting image small by making the window small.
      createWindow([fixPort(kURLBaseA + 'text.html')],
                   kWindowRect,
                   pass(function(winId, tabIds) {
        waitForAllTabs(pass(function() {
          chrome.tabs.query({active: true, windowId: winId},
                            pass(function(tabs) {
            // waitForAllTabs ensures this.
            assertEq('complete', tabs[0].status);
            chrome.tabs.captureVisibleTab(winId,
                                          {'format': 'png'},
                                          pass(function(imgDataUrl) {
              // The URL should be a data URL with has a PNG mime type.
              assertIsStringWithPrefix('data:image/png;base64,', imgDataUrl);

              textImageUrl = imgDataUrl;
              assertTrue(whiteImageUrl != textImageUrl);

              countPixelsWithColors(
                  imgDataUrl,
                  kWindowRect,
                  ['255,255,255,255'],
                  pass(function(colorCounts, totalPixelsChecked) {
                    // Some pixels should not be white, because the text
                    // is not white.  Can't test for black because
                    // antialiasing makes the pixels slightly different
                    // on each display setting.  Test that all pixels are
                    // not the same color.
                    assertTrue(totalPixelsChecked > colorCounts[0],
                               JSON.stringify({
                                 message: 'Some pixels should not be white.',
                                 totalPixelsChecked: totalPixelsChecked,
                                 numWhitePixels: colorCounts[0]
                               }, null, 2));
                  }));
              }));
          }));
        }));
      }));
    },

    function captureVisibleTabChromeExtensionScheme() {
      var url = chrome.runtime.getURL("/white.html");
      createWindow([url], kWindowRect, pass(function(winId, tabIds) {
        waitForAllTabs(pass(function() {
          chrome.tabs.query({active: true, windowId: winId},
                            pass(function(tabs) {
            // waitForAllTabs ensures this.
            assertEq('complete', tabs[0].status);
            chrome.tabs.captureVisibleTab(winId,
                                          {'format': 'png'},
                                          pass(function(imgDataUrl) {
              // The URL should be a data URL with has a PNG mime type.
              assertIsStringWithPrefix('data:image/png;base64,', imgDataUrl);
              testPixelsAreExpectedColor(imgDataUrl,
                                         kWindowRect,
                                         '255,255,255,255');  // White.
            }));
          }));
        }));
      }));
    }

  ])})
});
