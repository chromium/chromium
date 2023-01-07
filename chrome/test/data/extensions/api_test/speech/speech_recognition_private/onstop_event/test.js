// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testOnStop() {
  // The extension's client ID.
  const clientId = 4;
  chrome.speechRecognitionPrivate.onStop.addListener((event) => {
    if (event.clientId !== clientId) {
      chrome.test.sendMessage('Skipping event');
      return;
    }

    chrome.test.assertNoLastError();
    chrome.test.sendMessage('Processing event');
  });
  chrome.test.succeed();
}]);
