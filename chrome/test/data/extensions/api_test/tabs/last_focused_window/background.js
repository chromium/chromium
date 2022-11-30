// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getSelectedPopup() {
    chrome.windows.create(
        {type: 'popup', url: 'about:blank', focused: true},
        chrome.test.callbackPass(function(win) {
      chrome.windows.getLastFocused(
          chrome.test.callbackPass(function(lastFocusedWindowData) {
        chrome.test.assertTrue(lastFocusedWindowData.id == win.id);
      }));
    }));
  }
]);
