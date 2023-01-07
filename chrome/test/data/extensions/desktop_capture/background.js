/**
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// We need this thing because we can only run chooseDesktopMedia from inside
// the extension.

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    if (request.desktopSourceTypes) {
      chrome.desktopCapture.chooseDesktopMedia(request.desktopSourceTypes,
          sender.tab, function(id) {
        chrome.tabs.sendMessage(sender.tab.id, {streamId: id});
      });
    }
  });
