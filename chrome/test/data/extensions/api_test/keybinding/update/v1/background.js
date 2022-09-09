// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(windowId) {
  chrome.tabs.executeScript(null, { code: "document.body.bgColor='red'" });
});

chrome.test.notifyPass();
