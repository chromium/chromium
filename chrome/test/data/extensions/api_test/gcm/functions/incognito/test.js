// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testIncognito() {
    chrome.gcm.register(["Sender"], function(registrationId) {
      chrome.test.assertEq(chrome.runtime.lastError != undefined,
                           chrome.extension.inIncognitoContext);
      chrome.test.succeed();
    });
  }
]);
