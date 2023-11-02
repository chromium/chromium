// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testOnError() {
  chrome.speechRecognitionPrivate.onError.addListener((event) => {
    // The event should not have a clientId attached to it because the provided
    // key is the extension ID only (no client ID is provided).
    // See SpeechRecognitionPrivateManagerTest.DispatchOnErrorEvent for more
    // details.
    chrome.test.assertEq(undefined, event.clientId);
    chrome.test.assertEq('A fatal error', event.message);
    chrome.test.succeed();
  });

  // Triggers a fake speech recognition error from C++.
  chrome.test.sendMessage('Proceed');
}]);
