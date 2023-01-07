// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.getSelected(null, function(tab) {
  chrome.pageAction.show(tab.id);
  chrome.test.notifyPass();
});
