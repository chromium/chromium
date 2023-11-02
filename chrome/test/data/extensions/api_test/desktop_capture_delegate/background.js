// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, sendResponse) {
  if (message[0] == "getStream") {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        sender.tab,
        function(id) {
          sendResponse({"id": id});
        });
    return true;
  } else if (message[0] == "getStreamNoWait") {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        sender.tab, function() {});
    sendResponse({});
  } else {
    sendResponse({"error": "invalid message"});
  }
});
