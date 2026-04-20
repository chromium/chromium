// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function waitForMuteChangedEventTests() {
  chrome.test.listenOnce(chrome.audio.onMuteChanged, function(evt) {
    chrome.test.assertEq('INPUT', evt.streamType);
    chrome.test.assertTrue(evt.isMuted);
  });
}]);

chrome.test.sendMessage('loaded');
