// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Let the background page know this content script executed.
var inject = 'chrome.runtime.sendMessage({greeting: "hello"});';
chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  chrome.tabs.executeScript(tabId, {code:inject});
});

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    // Let the extension know where the script ran.
    var url = sender.tab ? sender.tab.url : 'about:blank';
    chrome.test.sendMessage('execute: ' + url);
  });

chrome.test.sendMessage('execute: ready');
