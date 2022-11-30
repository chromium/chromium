// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var listener = function(event) {
  var webview = document.querySelector('webview');

  // App code is not expected to redefine property on WebView.
  var canRedefineNameProperty = true;
  try {
    Object.defineProperty(webview, 'name', {
      get: function() { return 'foo'; },
      set: function(value) {},
      enumerable: true
    });
  } catch (e) {
    canRedefineNameProperty = false;
  }

  webview.addEventListener('loadstop', function(e) {
    chrome.test.runTests([
      function checkRedefineProperty() {
        chrome.test.assertFalse(canRedefineNameProperty);
        chrome.test.succeed();
      }
    ]);
  });

  webview.setAttribute('src', 'data:text/html,<body>Test</body>');
  event.target.removeEventListener(event.type, listener);
};

// Test when document.readyState changes to 'interactive'
document.addEventListener('readystatechange', listener);
