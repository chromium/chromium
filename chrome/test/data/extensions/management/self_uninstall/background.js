// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The name of the extension to uninstall, from manifest.json.
var EXPECTED_NAME = "Self Uninstall Test";

chrome.runtime.onInstalled.addListener(function() {
  chrome.management.getAll(function(items) {
    for (var i = 0; i < items.length; i++) {
      var item = items[i];
      if (item.name != EXPECTED_NAME) continue;
      chrome.management.uninstall(item.id);
    }
  });
});
