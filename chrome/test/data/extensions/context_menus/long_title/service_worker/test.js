// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make a 1000-character long title.
var title = '';
for (var i = 0; i < 1000; i++) {
  title += 'x';
}
chrome.test.log('creating item');
chrome.contextMenus.create({title: title, id: 'my_id'}, function() {
  if (!chrome.runtime.lastError) {
    chrome.test.sendMessage('created');
  }
});
