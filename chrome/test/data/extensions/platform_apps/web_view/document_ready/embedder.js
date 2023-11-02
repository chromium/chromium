// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function(e) {
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
    // Note that we are relying on .partition property to read the partition.
    // The other way would be to read this value from BrowserPluginGuest in cpp
    // code, which is slower and requires mode code (hurts readability of the
    // test).
    var partitionName = webview.partition;
    chrome.test.runTests([
      function checkRedefinePropertyAndPartitionCorrectness() {
        chrome.test.assertFalse(canRedefineNameProperty);
        chrome.test.assertEq('persist:test-partition', partitionName);
        chrome.test.succeed();
      }
    ]);
  });
  webview.partition = 'persist:test-partition';
  webview.setAttribute('src', 'data:text/html,<body>Test</body>');
});
