// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCreateTab() {
  chrome.tabs.create({url: 'about:blank'}, function(tab) {
    chrome.test.assertNoLastError();
    // Check that some tab properties are set. The exact values are not
    // important, just ensure they are valid.
    chrome.test.assertTrue(tab.id >= 0, 'id not valid');
    chrome.test.assertTrue(tab.windowId >= 0, 'windowId not valid');
    chrome.test.succeed();
  });
}

function testOnUpdated() {
  // TODO(crbug.com/371432155): When TabsEventRouterAndroid is implemented,
  // use chrome.tabs.onUpdated.addListener() to test it. For now, just succeed.
  chrome.test.succeed();
}

chrome.test.runTests([testCreateTab, testOnUpdated]);
