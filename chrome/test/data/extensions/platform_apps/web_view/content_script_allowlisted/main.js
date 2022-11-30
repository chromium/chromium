// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test makes sure webview properly loads and content script is run.
onload = function() {
  var contentScriptRan = false;
  var webviewLoaded = false;

  var maybePassTest = function() {
    if (contentScriptRan && webviewLoaded) {
      chrome.test.sendMessage('TEST_PASSED');
    }
  };

  var element = document.getElementById('the-bridge-element');
  if (element.innerText == 'Mutated') {
    contentScriptRan = true;
    maybePassTest();
  } else {
    // Wait for content script to fire an event.
    element.addEventListener('bridge-event', function(e) {
      contentScriptRan = true;
      maybePassTest();
    });
  }

  var webview = document.createElement('webview');
  // Load the webview twice so that we catch if there were any webview
  // renderer/ crashes during the first navigation.
  webview.onloadstop = function(e) {
    window.setTimeout(function() {
      webview.onloadstop = function() {
        webviewLoaded = true;
        maybePassTest();
      };
      // Loading the webview again exposes WebFrame::swap crash, otherwise
      // the test probably finishes too early.
      webview.src = 'about:blank';
    }, 0);
  };
  webview.setAttribute(
      'src', 'data:text/html,<html><body>tear down test</body></html>');
  document.body.appendChild(webview);
};
