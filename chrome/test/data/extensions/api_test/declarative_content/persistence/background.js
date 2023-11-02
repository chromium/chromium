// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var hostPrefix = chrome.extension.inIncognitoContext ? 'test_split' :
    'test_normal';

var rule = {
  conditions: [
    new chrome.declarativeContent.PageStateMatcher({
        pageUrl: {hostPrefix: hostPrefix}})
  ],
  actions: [
    new chrome.declarativeContent.ShowAction()
  ]
};

function sendMessage(message) {
  if (chrome.extension.lastError) {
    chrome.test.sendMessage(chrome.extension.lastError.message);
  } else {
    chrome.test.sendMessage(
        message + (chrome.extension.inIncognitoContext ? " (split)" : ""));
  }
}

// Make a one-time call to addRules.  We would register with
// chrome.runtime.onInstalled for this rather than recording in
// chrome.storage.local, but the onInstalled event only gets sent for the
// non-incognito side of the extension in split incognito mode.
var key = chrome.extension.inIncognitoContext ? "split" : "normal";
chrome.storage.local.get(key, function(items) {
  if (!(key in items)) {
    chrome.declarativeContent.onPageChanged.addRules([rule], function() {
      items[key] = "added";
      chrome.storage.local.set(items, sendMessage.bind(null, "ready"));
    });
  } else {
    sendMessage("second run ready");
  }
});
