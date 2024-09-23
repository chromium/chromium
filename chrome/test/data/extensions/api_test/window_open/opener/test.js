// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that even though window.opener is not set when using
  // chrome.windows.create directly, the same effect can be achieved by creating
  // a window, giving it a name, and then targetting that window via
  // window.open().
  function checkOpener() {
    // Make sure that we wait for the load callbacks to fire.
    var testCompleted = chrome.test.callbackAdded();
    var testWindowId;

    window.onSetNameLoaded = function(testWindow) {
      // It's not technically required for window.opener to be null when using
      // chrome.windows.create, but that is the current expected behavior (and
      // the reason why the window.name/open() workaround is necessary).
      chrome.test.assertTrue(testWindow.opener == null);
      window.open('check-opener.html', 'target-window');
    };

    window.onCheckOpenerLoaded = function(testWindow) {
      // The opener should now be set...
      chrome.test.assertNe(null, testWindow.opener);
      // ...and the test window should only have one tab (because it was
      // targeted via the "target-window" name).
      chrome.tabs.query(
          {windowId:testWindowId},
          chrome.test.callbackPass(function(tabs) {
            chrome.test.assertEq(1, tabs.length);
            chrome.test.assertEq(
                chrome.runtime.getURL('check-opener.html'), tabs[0].url);
            testCompleted();
          }));
    };

    chrome.windows.create(
        {'url': 'set-name.html'},
        chrome.test.callbackPass(function(win) {
          testWindowId = win.id;
        }));
  }
]);
