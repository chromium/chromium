// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([testEnqueue = async () => {
  let callbacks = 0;
  // Try synchronous callback-style API.
  chrome.tts.speak(
      'text 1', {
        'enqueue': true,
        'onEvent': event => {
          chrome.test.assertEq('end', event.type);
          callbacks++;
        }
      },
      () => {
        chrome.test.assertNoLastError();
        callbacks++;
      });
  // Try async promise-style API.
  await chrome.tts.speak('text 2', {
    'enqueue': true,
    'onEvent': event => {
      chrome.test.assertEq('end', event.type);
      callbacks++;
      if (callbacks == 4) {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    }
  });
  callbacks++;
}]);
