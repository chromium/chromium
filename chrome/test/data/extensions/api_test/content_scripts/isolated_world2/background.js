// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.log("got connect");
  port.onMessage.addListener(function(msg) {
    chrome.test.log("got message: " + msg);
    chrome.test.assertTrue(msg);
    chrome.test.notifyPass();
  });
});

chrome.windows.getCurrent(null, function(window) {
  chrome.tabs.query({windowId:window.id}, function(tabs) {
    chrome.test.log("Got tabs: " + JSON.stringify(tabs));

    // The last tab is the one that the other extension should have run scripts
    // in.
    chrome.tabs.executeScript(tabs.pop().id, {file: "a.js"});
  });
});
