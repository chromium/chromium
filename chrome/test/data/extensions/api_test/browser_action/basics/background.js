// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  // Privacy-sensitive properties are treated specially. Ensure they are
  // present.
  chrome.test.assertTrue(Boolean(tab.url.length));
  chrome.test.assertTrue(Boolean(tab.title.length));

  // Everything else is handled in a general way and should of course also
  // be present.
  chrome.test.assertTrue(Boolean(tab.id));
  chrome.test.notifyPass();
});

chrome.test.sendMessage('ready');
