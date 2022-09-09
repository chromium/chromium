// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var afterTabOpened = function() {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertTrue(!!stream);
    stream.getVideoTracks()[0].stop();
    stream.getAudioTracks()[0].stop();
    chrome.test.notifyPass();
  });
};

chrome.test.notifyPass();
chrome.test.sendMessage('ready1', afterTabOpened);
