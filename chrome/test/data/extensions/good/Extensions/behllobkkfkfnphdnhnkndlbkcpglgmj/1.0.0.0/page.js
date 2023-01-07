// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This function is called from the C++ browser test. It does a basic sanity
// test that we can call extension APIs.
function testTabsAPI() {
  chrome.tabs.getSelected(null, function(tab) {
    window.domAutomationController.send(
      tab.title == document.title && tab.url == location.href);
  });
}
