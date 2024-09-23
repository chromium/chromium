// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;

function pageUrl(letter) {
  return chrome.runtime.getURL(letter + ".html");
}

chrome.runtime.onMessage.addListener(function listener(tab,
                                                       sender,
                                                       sendResponse) {
  chrome.runtime.onMessage.removeListener(listener);
  assertEq(tab.url, pageUrl('a'));
  chrome.tabs.remove(tab.id, function() {
    chrome.test.succeed();
  });
});

chrome.test.runTests([
  function backgroundPageGetCurrentTab() {
    chrome.tabs.getCurrent(function(tab) {
      // There should be no current tab.
      assertEq(tab, undefined);
      chrome.test.succeed();
    });
  },

  function openedTabGetCurrentTab() {
    chrome.tabs.create({url: pageUrl("a")});
    // Completes in the onMessage listener, which is triggered by the
    // load of a.html.
  }
]);
