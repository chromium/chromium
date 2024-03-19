// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This function is called from the C++ browser test. It do a basic sanity
// test that we can call extension APIs.
function testTabsAPI() {
  return new Promise(resolve => {
    chrome.tabs.query({active: true}, function(tabs) {
      resolve(tabs[0].title == document.title && tabs[0].url == location.href);
    });
  });
}
