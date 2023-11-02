// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.browserAction.onClicked.addListener(function(tab) {
  chrome.tabs.create({ url: 'about:blank' }, function(newtab) {
    chrome.windows.get(tab.windowId, { populate: true }, function(window) {
      if (!window) {
        chrome.test.notifyFail(
            'Could not get window for the tab (probably due to wrong profile)');
        return;
      }
      chrome.test.assertEq(2, window.tabs.length);
      chrome.test.notifyPass();
    });
  });
});

let message = chrome.extension.inIncognitoContext ?
  'incognito ready' : 'regular ready';
chrome.test.sendMessage(message);
