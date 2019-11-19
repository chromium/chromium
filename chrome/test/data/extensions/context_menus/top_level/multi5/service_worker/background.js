// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contextMenus.create(
    {title: 'Context Menu #1', id: 'multi5_1'}, function() {
      if (!chrome.runtime.lastError) {
        chrome.contextMenus.create(
            {title: 'Context Menu #2', id: 'multi5_2'},
            function() {
              if (!chrome.runtime.lastError) {
                chrome.test.sendMessage('created items');
              }
            });
      }
});
