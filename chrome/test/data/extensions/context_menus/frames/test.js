// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


chrome.contextMenus.onClicked.addListener(function(info, tab) {
  chrome.test.sendMessage('pageUrl=' + info.pageUrl +
      ', frameUrl=' + info.frameUrl +
      ', frameId=' + info.frameId);
});

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.create(
      {title: 'Page item', contexts: ['page'], id: 'item1'},
      function() {
        if (!chrome.runtime.lastError) {
          chrome.contextMenus.create(
              {title: 'Frame item', contexts: ['frame'], id: 'frame_item'},
          function() {
            if (!chrome.runtime.lastError) {
              chrome.test.sendMessage('created items');
            }
          });
        }
      })});
