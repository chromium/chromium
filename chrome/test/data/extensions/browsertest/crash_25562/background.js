// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function() {
    // Let Chrome know that the PageAction needs to be enabled for this tabId
    // and for the url of this page.
    chrome.pageAction.show(port.sender.tab.id);
  });
});
