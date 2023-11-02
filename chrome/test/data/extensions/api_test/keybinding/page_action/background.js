// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user clicks on the page action.
chrome.pageAction.onClicked.addListener(function(tab) {
  chrome.test.sendMessage('clicked');
});

chrome.test.notifyPass();
