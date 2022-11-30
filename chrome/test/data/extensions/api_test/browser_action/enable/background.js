// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function enableBrowserAction(message) {
   if(message == 'start enabled') {
      chrome.browserAction.disable(function() {
          chrome.test.notifyPass();
      });
   } else {
      chrome.browserAction.enable(function() {
          chrome.test.notifyPass();
      });
   }
}

chrome.test.sendMessage('ready', function(message) {
  enableBrowserAction(message);
});
