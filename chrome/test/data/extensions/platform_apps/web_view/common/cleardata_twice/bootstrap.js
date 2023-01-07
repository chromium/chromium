// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// Guest served from TestServer.
config.IS_JS_ONLY_GUEST = false;
config.TEST_DIR = 'cleardata_twice';

var clearDataTwiceTests = {};

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  chrome.test.getConfig(function(chromeConfig) {
    window.console.log('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function() {
      chrome.test.runTests([
        clearDataTwiceTests.clear
      ]);
    }, function(data) {
      // We don't expect guest to send us any postMessage.
      chrome.test.fail();
    });
  });
};

// Tests.
// Clears http cache of a webview twice.
clearDataTwiceTests.clear = function testClearDataTwice() {
  console.log('clearDataTwiceTests.clear');
  embedder.webview.clearData({}, {cache: true}, function() {
    embedder.webview.clearData({}, {cache: true}, function() {
      chrome.test.succeed();
    });
  });
};

// Run test(s).
run();
