// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateBrowserAction() {
  chrome.browserAction.setTitle({title: 'Modified'}, function() {
    chrome.browserAction.setIcon({path: 'icon2.png'}, function() {
      chrome.browserAction.setBadgeText({text: 'badge'}, function() {
        chrome.browserAction.setBadgeBackgroundColor({color: [255,255,255,255]},
                                                     function() {
          chrome.test.notifyPass();
        });
      });
    });
  });
}

chrome.extension.isAllowedIncognitoAccess(function(isAllowedAccess) {
  switch(isAllowedAccess) {
    case false:
      chrome.test.sendMessage('incognito not allowed');
      break;
    case true:
      chrome.test.sendMessage('incognito allowed', function(message) {
        if (message == 'incognito update') {
          updateBrowserAction();
        }
      });
      break;
  }
});

chrome.test.sendMessage('ready', function(message) {
  if (message == 'update') {
    updateBrowserAction();
  }
});
