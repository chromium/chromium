// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Open a popup then return control to the ApiTest.
// The ApiTest can reply to the ready message to request another popup to open.
chrome.browserAction.openPopup(function(popupWindow) {
  chrome.test.assertTrue(!!popupWindow);
  chrome.test.notifyPass();
  chrome.test.sendMessage('ready', function(reply) {
    if (reply !== 'show another')
      return;
    chrome.browserAction.openPopup(function(popupWindow2) {
      chrome.test.assertTrue(!!popupWindow2);
      chrome.test.notifyPass();
    });
  });
});
