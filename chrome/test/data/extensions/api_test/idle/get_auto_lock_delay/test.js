// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
    function getAutoLockDelay() {
      chrome.idle.getAutoLockDelay(chrome.test.callbackPass(function (delay) {
        chrome.test.assertTrue(Number.isInteger(delay) && delay >= 0);
      }));
    }
  ]);
