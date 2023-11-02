// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions to help with tabs/windows testing.

// Removes current windows and creates one window with tabs set to
// the urls in the array |tabUrls|. At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function setupWindow(tabUrls, callback) {
  createWindow(tabUrls, {}, function(winId, tabIds) {
    // Remove all other windows.
    var removedCount = 0;
    chrome.windows.getAll({}, function(windows) {
      for (var i in windows) {
        if (windows[i].id != winId) {
          chrome.windows.remove(windows[i].id, function() {
            removedCount++;
            if (removedCount == windows.length - 1)
              callback(winId, tabIds);
          });
        }
      }
      if (windows.length == 1)
        callback(winId, tabIds);
    });
  });
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, winOptions, callback) {
  winOptions["url"] = tabUrls;
  chrome.windows.create(winOptions, function(win) {
    var newTabIds = [];
    assertTrue(win.id > 0);
    assertEq(tabUrls.length, win.tabs.length);

    for (var i = 0; i < win.tabs.length; i++)
      newTabIds.push(win.tabs[i].id);

    callback(win.id, newTabIds);
  });
}
