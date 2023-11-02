// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  const errorPromise = new Promise((resolve) => {
    chrome.speechRecognitionPrivate.onError.addListener((event) => {
      chrome.test.assertEq(
          'A speech recognition error occurred', event.message);
      resolve();
    });
  });

  const stopPromise = new Promise((resolve) => {
    chrome.speechRecognitionPrivate.onStop.addListener(() => resolve());
  });

  // Start speech recognition and send a message to C++, which will trigger a
  // fake speech recognition error to be fired.
  await chrome.speechRecognitionPrivate.start({});
  chrome.test.sendMessage('Started');

  // Verify that onError and onStop events are handled.
  Promise.all([errorPromise, stopPromise])
      .then(() => {
        chrome.test.succeed();
      })
      .catch(() => {
        chrome.test.fail();
      });
})();
