// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The name of the extension to uninstall, from manifest.json.
const EXPECTED_NAME = 'Self Uninstall Test';

chrome.management.onUninstalled.addListener(function(id) {
  chrome.management.getAll(function(items) {
    let found = false;
    for (let i = 0; i < items.length; i++) {
      chrome.test.assertNe(items.id, id);
      if (items[i].name !== EXPECTED_NAME) {
        continue;
      }
      found = true;
    }
    chrome.test.assertFalse(found);
    chrome.test.sendMessage('success');
  });
});

chrome.test.sendMessage('ready');
