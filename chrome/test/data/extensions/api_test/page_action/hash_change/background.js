// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This makes sure we only enable the page action once per tab.
const hasEnabled = {};

chrome.runtime.onMessage.addListener(function(request, sender) {
  if (request.msg === 'feedIcon') {
    console.info(`url: ${sender.tab.url}`);

    if (!hasEnabled[sender.tab.id]) {
      console.info(`Enabling for ${sender.tab.id}`);

      // We have received a list of feed urls found on the page.
      // Enable the page action icon.
      chrome.pageAction.setTitle(
          {tabId: sender.tab.id, title: 'Page action...'});
      chrome.pageAction.show(sender.tab.id);
      hasEnabled[sender.tab.id] = true;
      hasEnabledLastTabId = sender.tab.id;
    } else {
      console.info(
          `We are not doing this more than once (for ${sender.tab.id})`);
    }
  }
});
