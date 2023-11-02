// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {
    'left': 10,
    'top': 10,
    'width': 400,
    'height': 500,
    'frame': 'none'
  }, function(win) {
    chrome.test.log('Window opened callback.')
    // Send message after "load" event to ensure the <window-controls> shadow
    // element has had time to inject itself in the document.
    win.contentWindow.addEventListener('load', function() {
      chrome.test.log('onload event fired.')
      chrome.test.sendMessage('window-opened');
    });
    win.contentWindow.addEventListener('click', function(e) {
      chrome.test.log('click event fired at position (' + e.clientX + ',' +
        e.clientY +').')
    });
    // Send message to test when the window is closed (which should happen if
    // if the test simulated a left click at the right location).
    win.onClosed.addListener(function() {
      chrome.test.log('onClosed event fired.')
      chrome.test.sendMessage('window-closed');
    });
  });
});
