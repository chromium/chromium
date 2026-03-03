// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getInfoHasUsableDisplayMetadata() {
    chrome.system.display.getInfo(chrome.test.callbackPass(function(displays) {
      chrome.test.assertTrue(displays.length >= 1);

      let primaryCount = 0;
      let nonPrimaryCount = 0;
      const seenIds = new Set();

      for (const info of displays) {
        chrome.test.assertFalse(seenIds.has(info.id));
        seenIds.add(info.id);
        if (info.isPrimary) {
          primaryCount++;
        } else {
          nonPrimaryCount++;
        }

        chrome.test.assertTrue(info.id.length > 0);
        chrome.test.assertTrue(info.name.length > 0);
        chrome.test.assertTrue(info.dpiX > 0);
        chrome.test.assertTrue(info.dpiY > 0);
        chrome.test.assertTrue(info.bounds.width > 0);
        chrome.test.assertTrue(info.bounds.height > 0);
        chrome.test.assertTrue(info.workArea.width > 0);
        chrome.test.assertTrue(info.workArea.height > 0);
      }

      chrome.test.assertEq(1, primaryCount);

      if (displays.length > 1) {
        chrome.test.assertTrue(nonPrimaryCount >= 1);
      }
    }));
  },
]);
