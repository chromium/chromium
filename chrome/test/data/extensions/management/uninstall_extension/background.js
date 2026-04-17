// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The name of the extension to uninstall, from manifest.json.
const EXPECTED_NAME = 'Auto-Update Test';

chrome.management.getAll(function(items) {
  for (let i = 0; i < items.length; i++) {
    const item = items[i];
    if (item.name != EXPECTED_NAME)
      continue;
    var id = item.id;
    chrome.test.assertEq(false, item.mayDisable);
    chrome.test.runWithUserGesture(function() {
      chrome.management.uninstall(id, function() {
        // Check that the right error occured.
        const expectedError =
            'Extension ' + id + ' cannot be modified by user.';
        chrome.test.assertEq(expectedError, chrome.runtime.lastError.message);
        chrome.test.sendMessage('ready');
      });
    });
  }
});
