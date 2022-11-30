// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertState = function(win) {
  if (win.id == 'normal') {
    chrome.test.assertFalse(win.isMinimized());
    chrome.test.assertFalse(win.isMaximized());
  }
  if (win.id == 'maximized') {
    chrome.test.assertFalse(win.isMinimized());
    chrome.test.assertTrue(win.isMaximized());
  }
}

var testRestoreState = function(state_type) {
  chrome.app.window.create(
    'empty.html',
    { id: state_type, state: state_type },
    chrome.test.callbackPass(windowCreated)
  );
  function windowCreated(win) {
    assertState(win);
    win.onClosed.addListener(chrome.test.callbackPass(windowClosed));
    win.close();
    function windowClosed() {
      chrome.app.window.create(
        'empty.html',
        { id: state_type },
        function(win2) { assertState(win2); }
      );
    }
  };
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testRestoreNormal() {
      testRestoreState('normal');
    },
    function testRestoreMaximized() {
      testRestoreState('maximized');
    },
    // Minimize and fullscreen behavior are platform dependent.
  ]);
});
