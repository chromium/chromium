// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.downloads.search({}, function(items) {
  for (var i = 0; i < items.length; ++i) {
    if (items[i].state == 'in_progress' &&
        items[i].danger != 'safe' &&
        items[i].danger != 'accepted') {
      console.log(items[i].id);
      chrome.downloads.acceptDanger(items[i].id);
    }
  }
});
