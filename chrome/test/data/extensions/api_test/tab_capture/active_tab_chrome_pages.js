// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var afterGrantPermission = function() {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!stream);
    stream.getVideoTracks()[0].stop();
    stream.getAudioTracks()[0].stop();
    chrome.test.succeed();
  });
};

var afterOpenTab = function() {
  chrome.test.sendMessage('ready2', afterGrantPermission);
};

chrome.test.notifyPass();
chrome.test.sendMessage('ready1', afterOpenTab);
