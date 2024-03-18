// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.mimeHandlerPrivate.getStreamInfo(function(streamInfo) {
  chrome.test.assertEq(
      chrome.runtime.getURL('well-known-mime.ics'), streamInfo.originalUrl);
  var x = new XMLHttpRequest();
  x.open('GET', streamInfo.streamUrl);
  x.onloadend = function() {
    chrome.runtime.sendMessage('finish test by checking SW URLs');
  };
  x.send();
});
