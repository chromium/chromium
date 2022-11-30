// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNoLastError(message) {
  if (chrome.runtime.lastError) {
    message += ': ' + chrome.runtime.lastError.message;
  }
  chrome.test.assertFalse(!!chrome.runtime.lastError, message);
}

function executeWindowTest(testBody) {
  chrome.app.window.create(
      'appwindow.html', {}, function(appWindow) {
        assertNoLastError('window.create');
        appWindow.contentWindow.onload = function() {
          testBody(appWindow.contentWindow);
        };
      });
}

function createWebviews(doc, numWebviews, onWebviewsLoaded) {
  var webviewStates = {};
  for (var i = 0; i < numWebviews; i++) {
    // Initialize object with mapping from id to isLoaded boolean.
    webviewStates['webview' + i] = false;
  }
  function allLoaded() {
    return Object.values(webviewStates).every(function(isLoaded) {
      return isLoaded;
    });
  }
  function onLoadStop(e) {
    var webview = e.target;
    e.target.removeEventListener('loadstop', onLoadStop);
    var wasLoaded = allLoaded();
    webviewStates[webview.id] = true;
    var isLoaded = allLoaded();
    if (!wasLoaded && isLoaded) {
      onWebviewsLoaded();
    }
  }
  Object.keys(webviewStates).forEach(function(webviewId) {
    var webview = doc.createElement('webview');
    webview.src = 'data:text/plain, test';
    webview.addEventListener('loadstop', onLoadStop);
    webview.id = webviewId;
    doc.body.appendChild(webview);
  });
}

function testStartStopWithOneWebview() {
  executeWindowTest(function(win) {
    createWebviews(win.document, 1, function() {
      win.attemptAudioDebugRecording(
          function() {
            chrome.test.succeed('Started and stopped with 1 webview');
          }, chrome.test.fail);
    });
  });
}

function testFailWithMoreThanOneWebview() {
  executeWindowTest(function(win) {
    createWebviews(win.document, 2, function() {
      win.attemptAudioDebugRecording(
          function() {
            chrome.test.fail('Expected runtime error');
          },
          chrome.test.succeed);
    });
  });
}

function testFailWithZeroWebviews() {
  executeWindowTest(function(win) {
    win.attemptAudioDebugRecording(
        function() {
          chrome.test.fail('Expected runtime error');
        },
        chrome.test.succeed);
  });
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    testStartStopWithOneWebview,
    testFailWithMoreThanOneWebview,
    testFailWithZeroWebviews,
  ]);
});
