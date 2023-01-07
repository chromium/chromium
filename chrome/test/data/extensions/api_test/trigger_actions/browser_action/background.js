// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Turn the background red when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  chrome.tabs.executeScript(
      null, {code: "document.body.style.backgroundColor='red'"});
  chrome.test.notifyPass();
});

chrome.test.notifyPass();