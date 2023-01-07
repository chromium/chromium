// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    // Let the extension know where the script ran.
    var url = sender.tab ? sender.tab.url : 'about:blank';
    chrome.test.sendMessage('content script: ' + url);
  });
