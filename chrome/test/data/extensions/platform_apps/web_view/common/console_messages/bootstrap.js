// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'console_messages';

var consoleTests = {};

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  chrome.test.getConfig(function(chromeConfig) {
    window.console.log('getConfig: ' + chromeConfig);
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function() {
      chrome.test.runTests([
        consoleTests.testLogLog,
        consoleTests.testLogInfo,
        consoleTests.testLogWarn,
        consoleTests.testLogError,
        consoleTests.testLogDebug,
        consoleTests.testThrow
      ]);
    }, function(data) { return /* handled */ false; });
  });
};

consoleTests.testLogHelper_ = function(
    id, expectedLogLevel, expectedLogMessage) {
  var called = false;
  var logCallback = function(e) {
    embedder.webview.removeEventListener('consolemessage', logCallback);
    chrome.test.assertEq(expectedLogLevel, e.level);
    chrome.test.assertEq(expectedLogMessage, e.message);
    chrome.test.succeed();
  };
  embedder.webview.addEventListener('consolemessage', logCallback);
  embedder.webview.contentWindow.postMessage(JSON.stringify([id]), '*');
};

// Tests.
consoleTests.testLogLog = function testLogLog() {
  consoleTests.testLogHelper_('test-1a', 0, 'log-one-a');
};

consoleTests.testLogInfo = function testLogInfo() {
  consoleTests.testLogHelper_('test-1b', 0, 'log-one-b');
};

consoleTests.testLogWarn = function testLogWarn() {
  consoleTests.testLogHelper_('test-2', 1, 'log-two');
}

consoleTests.testLogError = function testLogError() {
  consoleTests.testLogHelper_('test-3', 2, 'log-three');
};

consoleTests.testLogDebug = function testLogDebug() {
  consoleTests.testLogHelper_('test-4', -1, 'log-four');
};

consoleTests.testThrow = function testThrow() {
  consoleTests.testLogHelper_('test-throw', 2, 'Uncaught Error: log-five');
};

// Run test(s).
run();
