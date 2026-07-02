// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('ready', function(reply) {
  if (reply === 'openPopup') {
    chrome.browserAction.openPopup(function(popup) {
      chrome.test.sendMessage(
          popup ? 'opened' :
              (chrome.runtime.lastError ?
                  chrome.runtime.lastError.message :
                  'failed'));
    });
  }
});
