// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // For these tests, ensure that the API is called with a locale of 'en-US'.
  // Otherwise, the the on-device variant of this test will fail because
  // on-device speech recognition is only supported in en-US.
  async function startAndStopTest() {
    await chrome.speechRecognitionPrivate.start(
      {locale: 'en-US', interimResults: true});
    await chrome.speechRecognitionPrivate.stop({});
    chrome.test.succeed();
  },

  async function startTwiceTest() {
    await chrome.speechRecognitionPrivate.start(
      {locale: 'en-US', interimResults: true});
    // Calling start() when speech recognition is active should trigger an
    // error.
    await chrome.test.assertPromiseRejects(
        chrome.speechRecognitionPrivate.start(
            {locale: 'en-US', interimResults: false}),
        'Error: Speech recognition already started');
    chrome.test.succeed();
  },

  async function stopWithoutStart() {
    await chrome.test.assertPromiseRejects(
      chrome.speechRecognitionPrivate.stop({}),
      'Error: Speech recognition already stopped');
    chrome.test.succeed();
  },
]);
