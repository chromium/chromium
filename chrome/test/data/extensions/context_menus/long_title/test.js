// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make a 1000-character long title.
var longTitle = '';
for (var i = 0; i < 1000; i++) {
  longTitle += 'x';
}

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.create({title: longTitle, id: 'my_id'}, function() {
    if (!chrome.runtime.lastError) {
      chrome.test.sendMessage('created');
    }
  })});
