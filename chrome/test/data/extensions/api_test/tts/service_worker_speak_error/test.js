// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([testSpeakError = async () => {
  // Async version.
  await chrome.tts
      .speak(
          'first try',
          {
            'lang': 'nevertheless',  // Invalid code will throw an error.
            'enqueue': true,
            'onEvent': (_event) => {
              chrome.test.fail();
            }
          })
      .catch(e => chrome.test.assertEq('Invalid lang.', e));
  // Sync version.
  chrome.tts.speak(
      'second try', {
        'enqueue': true,
        'rate': -5,  // Invalid rate will throw an error.
        'onEvent': (event) => {
          chrome.test.fail();
        }
      },
      function() {
        // Should have failed.
        chrome.test.assertEq('Invalid rate.', chrome.runtime.lastError.message);
        chrome.test.succeed();
      });
}]);
