// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getDeviceIdTest() {
    chrome.musicManagerPrivate.getDeviceId(function(id) {
      if (id !== undefined) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('string', typeof id);
        chrome.test.assertTrue(id.length >= 8);
      } else {
        // Bots may not support device ID. In this case, we still check that the
        // error message is what we expect.
        chrome.test.assertLastError(
            'Device ID API is not supported on this platform.');
      }
      chrome.test.succeed();
    });
  }
]);
