// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('ready', function() {
  chrome.browserAction.openPopup(function(popup) {
    chrome.test.assertFalse(!!popup);
    chrome.test.notifyPass();
  });
});
