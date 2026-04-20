// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function waitForLevelChangedEventTests() {
    chrome.test.listenOnce(chrome.audio.onLevelChanged, function (evt) {
      chrome.test.assertEq('30001', evt.deviceId);
      chrome.test.assertEq(60, evt.level);
    });
  }
]);

chrome.test.sendMessage('loaded');
