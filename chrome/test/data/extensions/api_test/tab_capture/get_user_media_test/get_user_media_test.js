// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var onReply = function(id) {
  chrome.test.runTests([
    function testGetUserMedia() {
      navigator.webkitGetUserMedia(
        {
          video:
          {
            'mandatory':
            {
              chromeMediaSource: 'tab',
              chromeMediaSourceId: id
            }
          },
          audio: false
        }, chrome.test.fail, chrome.test.succeed);
  }]);
};

chrome.test.notifyPass();
chrome.test.sendMessage('ready', onReply);