// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clear out items from previous test.
chrome.contextMenus.removeAll();

// Create a bunch of items underneath an explicit parent.
var parent = chrome.contextMenus.create({"title":"parent"}, function() {
  createTestSet(parent, function() {
    chrome.test.sendMessage("test2 create finished");
  });
});

