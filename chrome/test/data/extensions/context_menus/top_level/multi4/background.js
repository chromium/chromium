// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.create(
      {title: 'Context Menu #1', id: 'multi4_1'}, function() {
        if (!chrome.runtime.lastError) {
          chrome.contextMenus.create(
              {title: 'Context Menu #2', id: 'multi4_2'},
              function() {
                if (!chrome.runtime.lastError) {
                  chrome.test.sendMessage('created items');
                }
              });
        }
  });
});
