// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.extension.onRequest.addListener(function(msg, sender, responseFunc) {
  if (msg == "getApi") {
    responseFunc(chrome.test.getApiDefinitions());
  } else if (msg == "pass") {
    chrome.test.notifyPass();
  } else if (msg.substr(0, 3) == "log") {
    chrome.test.log(msg);
  } else {
    chrome.test.notifyFail("failed");
  }
});

// On first install, send a success message so the test can continue.
chrome.test.notifyPass();
