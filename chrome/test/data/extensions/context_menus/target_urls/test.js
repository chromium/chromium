// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var patterns = [ "http://*.google.com/*" ];

chrome.contextMenus.create({"title":"item1", "contexts": ["link"],
                            "targetUrlPatterns": patterns}, function() {
  if (!chrome.runtime.lastError) {
    chrome.test.sendMessage("created items");
  }
});
