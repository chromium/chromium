// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedLastErrorMessage =
    ('Extension has not been invoked for the current page (see activeTab ' +
     'permission). Chrome pages cannot be captured.');

var afterWhitelistExtension = function(msg) {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!stream);
    stream.getVideoTracks()[0].stop();
    stream.getAudioTracks()[0].stop();
    chrome.test.succeed();
  });
};

var afterOpenNewTab = function(msg) {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertLastError(expectedLastErrorMessage);
    chrome.test.assertTrue(!stream);
    chrome.test.sendMessage('ready4', afterWhitelistExtension);
  });
};

var afterGrantPermission = function(msg) {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!stream);
    stream.getVideoTracks()[0].stop();
    stream.getAudioTracks()[0].stop();
    chrome.test.sendMessage('ready3', afterOpenNewTab);
  });
};

var afterOpenTab = function(msg) {
  chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
    chrome.test.assertLastError(expectedLastErrorMessage);
    chrome.test.assertTrue(!stream);

    chrome.test.sendMessage('ready2', afterGrantPermission);
  });
};

chrome.test.notifyPass();
chrome.test.sendMessage('ready1', afterOpenTab);
