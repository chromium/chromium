// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'execute_script';
// We want to track loadstop event only once. Because we load
// an <iframe> element dynamically, that results in additional
// loadstop events.
config.SKIP_MULTIPLE_LOADSTOP = true; // We want to track 'loa

var executeScriptTests = {};
var g_webview;

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);
  var webviewLoaded = false;

  chrome.test.getConfig(function(chromeConfig) {
    window.console.log('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function(webview) {
      g_webview = webview;
      chrome.test.runTests([
        executeScriptTests.testExecuteScriptInAllFrames
      ]);
    }, function(data) {
      LOG('embedder.onPostMessageReceived, data[0] = ' + data[0]);
      switch (data[0]) {
        case 'created-frame':
          // execute script.
          g_webview.executeScript(
              {
                code: 'document.getElementById("testDiv").innerText += 42',
                allFrames: true
              },
              function(results) {
                if (!results || !results.length) {
                  window.console.log('results failure: ' + results);
                  chrome.test.fail();
                  return;
                }
                g_webview.contentWindow.postMessage(
                  JSON.stringify(['get-testDiv-innerText']), '*');
              });
          return true;
        case 'got-testDiv-innerText':
          chrome.test.assertEq('guest:42', data[1]);
          chrome.test.assertEq('frame:42', data[2]);
          chrome.test.succeed();
          return true;
        default:
          LOG('curious message: ' + data[0]);
          return false; // Will result in test failure.
      }
    }, 'foobar');
  });
};

// This test creates a <webview> which has a <div id="testDiv"> and that
// guest also has an <iframe> that contains a <div id="testDiv">.
// We executeScript on the <webview> to run on "allFrames", and check that
// the content modified by the script is reflected on both the <webview>
// and the frame inside it.
executeScriptTests.testExecuteScriptInAllFrames =
    function testExecuteScriptInAllFrames() {
  g_webview.contentWindow.postMessage(JSON.stringify(['create-frame']), '*');
};

// Run tests.
run();
