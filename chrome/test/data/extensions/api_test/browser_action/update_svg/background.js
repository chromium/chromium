// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var inServiceWorker = 'ServiceWorkerGlobalScope' in self;

function updateBrowserAction() {
  chrome.browserAction.setTitle({title: 'Modified'}, function() {
    chrome.browserAction.setIcon({path: 'icon2.svg'}, function() {
      if (inServiceWorker) {
        chrome.test.notifyFail(chrome.runtime.lastError.message);
      } else {
        chrome.test.assertNoLastError();
        chrome.browserAction.setBadgeText({text: 'badge'}, function() {
          chrome.browserAction.setBadgeBackgroundColor(
              {color: [255,255,255,255]}, function() {
            chrome.test.notifyPass();
          });
        });
      }
    });
  });
}

chrome.extension.isAllowedIncognitoAccess(function(isAllowedAccess) {
  if (isAllowedAccess == true) {
    chrome.test.sendMessage('incognito ready', function(message) {
      if (message == 'incognito update') {
        updateBrowserAction();
      }
    });
  }
});

chrome.test.sendMessage('ready', function(message) {
  if (message == 'update') {
    updateBrowserAction();
  }
});
