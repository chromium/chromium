// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getInfoDeterministicMultiDisplay() {
    chrome.system.display.getInfo(chrome.test.callbackPass(function(displays) {
      chrome.test.assertEq(3, displays.length);

      let primaryCount = 0;
      const seenIds = new Set();

      // Must match AndroidMultiDisplayMockProvider::GetAllDisplaysInfo() in
      // chrome/browser/extensions/api/system_display/
      // system_display_extension_apitest.cc
      const expectedDisplays = {
        '111': {
          name: 'Primary Monitor',
          dpiX: 160.0,
          dpiY: 160.0,
          isPrimary: true,
        },
        '222': {
          name: 'External Display 1',
          dpiX: 96.0,
          dpiY: 96.0,
          isPrimary: false,
        },
        '333': {
          name: 'External Display 2',
          dpiX: 320.0,
          dpiY: 320.0,
          isPrimary: false,
        },
      };

      for (const info of displays) {
        chrome.test.assertFalse(seenIds.has(info.id));
        seenIds.add(info.id);

        if (info.isPrimary) {
          primaryCount++;
        }

        const expected = expectedDisplays[info.id];
        chrome.test.assertTrue(!!expected);
        chrome.test.assertEq(expected.name, info.name);
        chrome.test.assertEq(expected.dpiX, info.dpiX);
        chrome.test.assertEq(expected.dpiY, info.dpiY);
        chrome.test.assertEq(expected.isPrimary, info.isPrimary);
      }

      chrome.test.assertEq(1, primaryCount);
    }));
  },
]);
