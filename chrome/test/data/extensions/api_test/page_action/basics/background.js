// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user clicks on the browser action.
chrome.pageAction.onClicked.addListener(function(windowId) {
  chrome.test.notifyPass();
});

chrome.test.notifyPass();
