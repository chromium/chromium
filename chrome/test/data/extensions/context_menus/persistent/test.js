// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var menuId = 'my_id'

chrome.runtime.onInstalled.addListener(function(details) {
  if (details.reason == 'install') {
    chrome.contextMenus.create(
        {title: 'Extension Item', id: menuId},
        function() {
          if (!chrome.runtime.lastError) {
            chrome.test.notifyPass();
          } else {
            chrome.test.notifyFail(chrome.runtime.lastError.message);
          }
        }
    );
  }
});

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  // The C++ test creates a tab at chrome://version as a signal to the
  // extension to update the menu item.
  if (tab.url != 'chrome://version/')
   return;
  chrome.contextMenus.update(
      menuId, {title: 'Extension Item Updated'},
      function() {
        if (!chrome.runtime.lastError) {
          chrome.test.notifyPass();
        } else {
          chrome.test.notifyFail(chrome.runtime.lastError.message);
        }
      }
  );
});
