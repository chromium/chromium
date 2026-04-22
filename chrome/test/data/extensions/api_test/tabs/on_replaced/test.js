// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.tabs.create({'url': 'about:blank'}, function(tab) {
    chrome.test.runTests([
      function onReplacedEvent() {
        const tabId = tab.id;

        const onReplaceListener = function(new_tab_id, old_tab_id) {
          chrome.test.assertNe(new_tab_id, tabId);
          chrome.test.assertEq(tabId, old_tab_id);
          chrome.tabs.onReplaced.removeListener(onReplaceListener);
          chrome.test.succeed();
        };
        chrome.tabs.onReplaced.addListener(onReplaceListener);

        chrome.test.notifyPass();
      },
    ]);
  });
});
