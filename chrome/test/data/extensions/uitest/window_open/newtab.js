// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testExtensionApi() {
  try {
    return new Promise(resolve => {
      chrome.windows.getCurrent(null, window => {
        chrome.tabs.query({windowId: window.id}, function() {
          resolve(!chrome.runtime.lastError);
        })
      });
    });
  } catch (e) {
    return false;
  }
}
