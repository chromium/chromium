// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var patterns = [ 'http://*.google.com/*' ];

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.create({title: 'item1', contexts: ['link'], id: 'my_id',
                              targetUrlPatterns: patterns}, function() {
    if (!chrome.runtime.lastError) {
      chrome.test.sendMessage('created items');
    }
  });
});
