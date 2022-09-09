// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onBeforeRequest.addListener(({tabId, url}) => {
  chrome.test.getConfig(function(config) {
    const expectedUrl =
          'http://127.0.0.1:' + config.testServer.port + '/empty.html';
    chrome.test.assertEq(expectedUrl, url);
    chrome.test.notifyPass();
  });
}, { urls: ["<all_urls>"], types: ["main_frame"] }, []);

// Tell the C++ side of the test that the listener was added.
chrome.test.sendMessage('listener-added');
