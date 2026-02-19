// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
    function getInfoHasUsableDisplayMetadata() {
        chrome.system.display.getInfo(
            chrome.test.callbackPass(function(displays) {
                chrome.test.assertTrue(displays.length >= 1);
                for (const info of displays) {
                    chrome.test.assertTrue(info.id.length > 0);
                    chrome.test.assertTrue(info.name.length > 0);
                    chrome.test.assertTrue(info.dpiX > 0);
                    chrome.test.assertTrue(info.dpiY > 0);
                    chrome.test.assertTrue(info.bounds.width > 0);
                    chrome.test.assertTrue(info.bounds.height > 0);
                    chrome.test.assertTrue(info.workArea.width > 0);
                    chrome.test.assertTrue(info.workArea.height > 0);
                }
            }
        ));
    },
]);