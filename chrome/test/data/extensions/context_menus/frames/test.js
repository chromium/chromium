// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onclick(info, tab) {
  chrome.test.sendMessage('pageUrl=' + info.pageUrl +
      ', frameUrl=' + info.frameUrl +
      ', frameId=' + info.frameId);
}

chrome.contextMenus.create(
    {'title':'Page item', contexts: ['page'], id: 'item1', onclick: onclick},
    function() {
      if (!chrome.runtime.lastError) {
        chrome.contextMenus.create(
        {"title":"Frame item", contexts: ["frame"]},
        function() {
          if (!chrome.runtime.lastError) {
            chrome.test.sendMessage("created items");
          }
        });
      }
    });
