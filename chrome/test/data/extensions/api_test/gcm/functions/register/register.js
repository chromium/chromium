// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testRegister() {
    var senderIds = ["Sender1", "Sender2"];
    chrome.gcm.register(senderIds, function(registrationId) {
      chrome.test.succeed();
    });
  }
]);
