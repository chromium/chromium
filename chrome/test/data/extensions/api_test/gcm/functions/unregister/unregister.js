// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testUnregister() {
    var senderIds = ["Sender1", "Sender2"];
    chrome.gcm.register(senderIds, function(registrationId) {
      if (chrome.runtime.lastError)
        chrome.test.fail();
      chrome.gcm.unregister(function() {
        if (chrome.runtime.lastError)
          chrome.test.fail();
        else
          chrome.test.succeed();
      });
    });
  },
  function testUnregisterWithServerError() {
    chrome.gcm.unregister(function() {
      if (chrome.runtime.lastError != undefined &&
          chrome.runtime.lastError.message == "Server error occurred.") {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    });
  }
]);
