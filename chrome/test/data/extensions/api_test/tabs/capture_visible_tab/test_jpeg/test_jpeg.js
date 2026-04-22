// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), capturing JPEG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabJpeg

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

const WINDOW_RECT = {
  width: 400,
  height: 400,
};

const TEST_DIR = '/extensions/api_test/tabs/capture_visible_tab/test_jpeg/';
const URL_BASE_A = `http://a.com:PORT${TEST_DIR}`;

// Globals used to allow a test to read data from a previous test.
let blackImageUrl;
let whiteImageUrl;

const scriptUrl =
    '_test_resources/api_test/tabs/capture_visible_tab/common/tabs_util.js';
const loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(() => {
  chrome.test.getConfig(function(config) {
    const fixPort = function(url) {
      return url.replace(/PORT/, config.testServer.port);
    };

    chrome.test.runTests([
      // Open a window with one tab, take a snapshot.
      function captureVisibleTabWhiteImage() {
        // Keep the resulting image small by making the window small.
        createWindow(
            [fixPort(`${URL_BASE_A}white.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      // waitForAllTabs ensures this.
                      assertEq('complete', tabs[0].status);
                      chrome.tabs.captureVisibleTab(
                          winId, pass(function(imgDataUrl) {
                            // The URL should be a data URL with has a JPEG mime
                            // type.
                            assertIsStringWithPrefix(
                                'data:image/jpeg;base64,', imgDataUrl);
                            whiteImageUrl = imgDataUrl;

                            testPixelsAreExpectedColor(
                                whiteImageUrl, WINDOW_RECT,
                                '255,255,255,255');  // White.
                          }));
                    }));
              }));
            }));
      },

      function captureVisibleTabBlackImage() {
        // Keep the resulting image small by making the window small.
        createWindow(
            [fixPort(`${URL_BASE_A}black.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      // waitForAllTabs ensures this.
                      assertEq('complete', tabs[0].status);
                      chrome.tabs.captureVisibleTab(
                          winId, pass(function(imgDataUrl) {
                            // The URL should be a data URL with has a JPEG mime
                            // type.
                            assertIsStringWithPrefix(
                                'data:image/jpeg;base64,', imgDataUrl);
                            blackImageUrl = imgDataUrl;

                            // Check that previous capture was done.
                            assertEq('string', typeof (whiteImageUrl));
                            assertTrue(whiteImageUrl != blackImageUrl);
                            testPixelsAreExpectedColor(
                                blackImageUrl, WINDOW_RECT,
                                '0,0,0,255');  // Black.
                          }));
                    }));
              }));
            }));
      },

      function captureVisibleTabChromeExtensionScheme() {
        const url = chrome.runtime.getURL('/white.html');
        createWindow([url], WINDOW_RECT, pass(function(winId, tabIds) {
                       waitForAllTabs(pass(function() {
                         chrome.tabs.query(
                             {active: true, windowId: winId},
                             pass(function(tabs) {
                               // waitForAllTabs ensures this.
                               assertEq('complete', tabs[0].status);
                               chrome.tabs.captureVisibleTab(
                                   winId, pass(function(imgDataUrl) {
                                     // The URL should be a data URL with has a
                                     // JPEG mime type.
                                     assertIsStringWithPrefix(
                                         'data:image/jpeg;base64,', imgDataUrl);
                                     testPixelsAreExpectedColor(
                                         imgDataUrl, WINDOW_RECT,
                                         '255,255,255,255');  // White.
                                   }));
                             }));
                       }));
                     }));
      },

      // Passing options with no format should default to jpeg.
      function captureVisibleTabNoFormat() {
        // Keep the resulting image small by making the window small.
        createWindow(
            [fixPort(`${URL_BASE_A}white.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      // waitForAllTabs ensures this.
                      assertEq('complete', tabs[0].status);
                      chrome.tabs.captureVisibleTab(
                          winId, {quality: 100}, pass(function(imgDataUrl) {
                            // The URL should be a data URL with has a JPEG mime
                            // type.
                            assertIsStringWithPrefix(
                                'data:image/jpeg;base64,', imgDataUrl);
                            whiteImageUrl = imgDataUrl;

                            testPixelsAreExpectedColor(
                                whiteImageUrl, WINDOW_RECT,
                                '255,255,255,255');  // White.
                          }));
                    }));
              }));
            }));
      },

      function captureVisibleTabWithRect() {
        const rect = {x: 10, y: 20, width: 80, height: 60};
        createWindow(
            [fixPort(`${URL_BASE_A}white.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      assertEq('complete', tabs[0].status);

                      // The captured image will be in physical pixels, so
                      // we need to scale our expected dimensions by the
                      // device pixel ratio.
                      chrome.test.sendMessage(
                          'get_device_pixel_ratio',
                          pass(function(devicePixelRatioStr) {
                            const devicePixelRatio =
                                parseFloat(devicePixelRatioStr);

                            chrome.tabs.captureVisibleTab(
                                winId, {format: 'jpeg', rect: rect},
                                pass(function(imgDataUrl) {
                                  assertIsStringWithPrefix(
                                      'data:image/jpeg;base64,', imgDataUrl);
                                  testPixelsAreExpectedColor(
                                      imgDataUrl, {
                                        width: Math.ceil(
                                            rect.width * devicePixelRatio),
                                        height: Math.ceil(
                                            rect.height * devicePixelRatio),
                                      },
                                      '255,255,255,255');  // White.
                                }));
                          }));
                    }));
              }));
            }));
      },

      function captureVisibleTabWithRectAndScale() {
        const rect = {x: 10, y: 20, width: 80, height: 60};
        const scale = 2.0;
        createWindow(
            [fixPort(`${URL_BASE_A}white.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      assertEq('complete', tabs[0].status);

                      // The captured image will be in physical pixels, so
                      // we need to scale our expected dimensions by the
                      // scale factor.
                      chrome.tabs.captureVisibleTab(
                          winId, {format: 'jpeg', rect: rect, scale: scale},
                          pass(function(imgDataUrl) {
                            assertIsStringWithPrefix(
                                'data:image/jpeg;base64,', imgDataUrl);
                            testPixelsAreExpectedColor(
                                imgDataUrl, {
                                  width: Math.ceil(rect.width * scale),
                                  height: Math.ceil(rect.height * scale),
                                },
                                '255,255,255,255');  // White.
                          }));
                    }));
              }));
            }));
      },

      function captureVisibleTabWithRectAndLargeScale_CheckNoOverflow() {
        const rect = {x: 10, y: 20, width: 80, height: 60};
        const scale = 2000000000;
        createWindow(
            [fixPort(`${URL_BASE_A}white.html`)], WINDOW_RECT,
            pass(function(winId, tabIds) {
              waitForAllTabs(pass(function() {
                chrome.tabs.query(
                    {active: true, windowId: winId}, pass(function(tabs) {
                      assertEq('complete', tabs[0].status);

                      // Since the scale factor is extremely large, the C++
                      // browser-side code will clamp the requested image
                      // dimensions to a safe maximum (INT_MAX). This results
                      // in an empty source rectangle, which is a special case
                      // that triggers a capture of the entire visible tab.
                      chrome.test.sendMessage(
                          'get_device_pixel_ratio',
                          pass(function(devicePixelRatioStr) {
                            const devicePixelRatio =
                                parseFloat(devicePixelRatioStr);

                            chrome.tabs.captureVisibleTab(
                                winId,
                                {format: 'jpeg', rect: rect, scale: scale},
                                function(imgDataUrl) {
                                  assertIsStringWithPrefix(
                                      'data:image/jpeg;base64,', imgDataUrl);

                                  fetch(imgDataUrl)
                                      .then(res => res.blob())
                                      .then(blob => createImageBitmap(blob))
                                      .then(pass(imageBitmap => {
                                        assertEq(
                                            Math.ceil(
                                                WINDOW_RECT.width *
                                                devicePixelRatio),
                                            imageBitmap.width,
                                            'Image width should match window width');
                                        assertEq(
                                            Math.ceil(
                                                WINDOW_RECT.height *
                                                devicePixelRatio),
                                            imageBitmap.height,
                                            'Image height should match window height');
                                      }))
                                      .catch(fail(e => {
                                        return `Checking image dimensions failed: ${
                                            e}`;
                                      }));
                                });
                          }));
                    }));
              }));
            }));
      },
    ]);
  });
});
