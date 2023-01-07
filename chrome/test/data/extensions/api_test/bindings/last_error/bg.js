// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function fail() {
  window.domAutomationController.send(false);
  throw "Failed!";
}

function succeed() {
  window.domAutomationController.send(true);
}

function testLastError() {
  // Make sure lastError is not yet set
  if (chrome.tabs.lastError)
    fail();

  var maxTabId = 0;

  // Find the highest tab id
  chrome.windows.getAll({populate:true}, function(windows) {
    // Make sure lastError is still not set. (this call have should succeeded).
    if (chrome.tabs.lastError)
      fail();

    for (var i = 0; i < windows.length; i++) {
      var win = windows[i];
      for (var j = 0; j < win.tabs.length; j++) {
        var tab = win.tabs[j];
        if (tab.id > maxTabId)
          maxTabId = tab.id;
      }
    }

    // Now ask for the next highest tabId.
    chrome.tabs.get(maxTabId + 1, function(tab) {
      // Make sure lastError *is* set and tab is not.
      if (!chrome.runtime.lastError ||
          !chrome.runtime.lastError.message ||
          tab)
        fail();

      window.setTimeout(finish, 10);
    });
  });
}

function finish() {
  // Now make sure lastError is unset outside the callback context.
  if (chrome.tabs.lastError)
    fail();

  succeed();
}

chrome.test.sendMessage('ready');
