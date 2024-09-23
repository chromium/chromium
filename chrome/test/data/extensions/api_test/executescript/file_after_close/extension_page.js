// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.windows.getCurrent(null, function(window) {
    chrome.tabs.query({windowId:window.id}, function(tabs) {
      for (var i = 0; i < tabs.length; i++) {
        var tab = tabs[i];
        if (tab.url.indexOf('web_page1') > -1) {
          chrome.tabs.executeScript(tab.id, { file: 'script.js' });
          window.close();
          break;
        }
      }
    });
  });
}
