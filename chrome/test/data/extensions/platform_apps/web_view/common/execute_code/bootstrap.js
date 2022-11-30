// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'execute_code';

var executeCodeTests = {};
var g_webview;

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  chrome.test.getConfig(function(chromeConfig) {
    window.console.log('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    var step = 1;
    embedder.loadGuest(function(webview) {
      g_webview = webview;
      window.console.log('bootstrap got embedder.loadGuest');
      chrome.test.runTests([
        executeCodeTests.testInsertCSS
      ]);
    }, function(data) {
      LOG('embedder.onPostMessageReceived, data[0] = ' + data[0]);
      if (data[0] == 'style') {
        var propertyName = data[1];
        var value = data[2];

        switch (step) {
          case 1:
            chrome.test.assertEq('background-color', propertyName);
            chrome.test.assertEq('rgba(0, 0, 0, 0)', value);
            testBackgroundColorAfterCSSInjection();
            step = 2;
            break;
          case 2:
            chrome.test.assertEq('background-color', propertyName);
            chrome.test.assertEq('rgb(255, 0, 0)', value);
            chrome.test.succeed();
            break;
          default:
            break;
        }
        return true;
      }
      return false;
    }, 'foobar');
  });
};

var testBackgroundColorAfterCSSInjection = function() {
  LOG('testBackgroundColorAfterCSSInjection');
  g_webview.insertCSS({file: 'execute_code/guest.css'}, function (results) {
    // Verify that the background color is now red after injecting
    // the CSS file.
    LOG('testBackgroundColorAfterCSSInjection second postMessage send');
    g_webview.contentWindow.postMessage(
        JSON.stringify(['get-style', 'background-color']), '*');
  });
};

executeCodeTests.testInsertCSS = function testInsertCSS() {
  // Test the background color before CSS injection. Verify that the background
  // color is indeed white.
  g_webview.contentWindow.postMessage(
      JSON.stringify(['get-style', 'background-color']), '*');
};

// Run tests.
run();
