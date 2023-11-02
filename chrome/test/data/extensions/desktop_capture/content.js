/**
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    // Callback from extension, ChooseDesktopMedia has completed.
    window.postMessage(request, "*");
});

window.addEventListener('message', function(event) {
  if (event.source != window || !event.data) {
    return;
  }

  // Send request to extension. event.data contains desktopSourceTypes.
  chrome.runtime.sendMessage(event.data);
});
