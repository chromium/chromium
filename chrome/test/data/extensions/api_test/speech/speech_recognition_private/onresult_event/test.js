// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testOnResult() {
  const firstClientId = 1;
  const secondClientId = 2;

  chrome.speechRecognitionPrivate.onResult.addListener((event) => {
    if (event.clientId !== firstClientId) {
      chrome.test.sendMessage('Skipping event in first listener');
      return;
    }

    chrome.test.assertEq('This is a test', event.transcript);
    chrome.test.assertFalse(event.isFinal);
    chrome.test.sendMessage('Received result');
  });

  chrome.speechRecognitionPrivate.onResult.addListener((event) => {
    if (event.clientId !== secondClientId) {
      chrome.test.sendMessage('Skipping event in second listener');
      return;
    }

    chrome.test.assertEq('This is a test', event.transcript);
    chrome.test.assertTrue(event.isFinal);
    chrome.test.sendMessage('Received result');
  });

  chrome.test.succeed();
}]);
