// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var contextMenuTitle = 'Context Menu #2 - Extension #3';

chrome.contextMenus.create(
    {title: contextMenuTitle, id: 'single3_1'}, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.sendMessage('created item');
      }
});
