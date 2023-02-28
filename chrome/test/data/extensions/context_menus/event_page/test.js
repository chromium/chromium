// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener(function() {
  chrome.contextMenus.create({'id': 'item1', 'title': 'Item 1'});
  chrome.contextMenus.create(
      {'id': 'checkbox1', 'title': 'Checkbox 1', 'type': 'checkbox'});
});

chrome.contextMenus.onClicked.addListener(function(info, tab) {
  chrome.test.assertNe(null, tab.id);
  chrome.test.assertEq(0, info.frameId); // 0 = main frame
  chrome.test.sendMessage("onClicked fired for " + info.menuItemId);
});
