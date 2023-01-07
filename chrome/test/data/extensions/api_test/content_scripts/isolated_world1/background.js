// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.log("got connect");
  port.onMessage.addListener(function(msg) {
    chrome.test.log("got message: " + msg);
    chrome.test.assertEq(2, msg);
    chrome.test.notifyPass();
  });
});

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  chrome.test.log("Got update event: " + JSON.stringify(changeInfo));
  if (changeInfo.status == "complete") {
    chrome.tabs.executeScript(tabId, {file: "c.js"});
    chrome.tabs.onUpdated.removeListener(arguments.callee);
  }
});

chrome.test.getConfig(function(config) {
  chrome.test.log("Creating tab...");
  chrome.tabs.create({
    url: "http://localhost:PORT/extensions/test_file.html"
             .replace(/PORT/, config.testServer.port)
  });
});
