// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
chrome.tabs.getCurrent((tab) => {
  chrome.tabs.group({tabIds: tab.id}, (groupId) => {
    chrome.tabGroups.update(groupId, {title: '🤡'}, (group) => {
      chrome.test.notifyPass();
    });
  });
});
