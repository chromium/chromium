// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function windowSetFocused() {
    chrome.windows.getCurrent(function(oldWin) {
      chrome.test.assertTrue(oldWin.focused);
      var focusIds = [];
      var newWindowId;
      // Listen to verify the change events come in the order expected.
      chrome.windows.onFocusChanged.addListener(function listener(windowId) {
        // Events may be sent when all Chrome windows have lost focus,
        // so ignore those.
        if (windowId == chrome.windows.WINDOW_ID_NONE)
          return;
        focusIds.push(windowId);
        if (focusIds.length == 2) {
          chrome.windows.onFocusChanged.removeListener(listener);
          chrome.test.assertEq(newWindowId, focusIds[0]);
          chrome.test.assertEq(oldWin.id, focusIds[1]);
          chrome.test.succeed();
        }
      });
      // Create a new window, update its focus, then return focus to the
      // original window.
      chrome.windows.create({}, function(newWin) {
        newWindowId = newWin.id;
        chrome.windows.update(newWin.id, {focused:true}, function(win) {
          chrome.test.assertEq(newWin.id, win.id);
          chrome.test.assertTrue(win.focused);
          chrome.windows.update(oldWin.id, {focused:true}, function(win) {
            chrome.test.assertEq(oldWin.id, win.id);
            chrome.test.assertTrue(win.focused);
          });
        });
      });
    });
  },
]);
