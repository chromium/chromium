// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.browserAction.onClicked.addListener(function(tab) {
  chrome.browserAction.setBadgeText({text: 'X'}, () => {
    window.close();
  });
});

chrome.test.sendMessage('ready');
