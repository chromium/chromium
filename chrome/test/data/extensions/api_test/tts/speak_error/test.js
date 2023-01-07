// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testSpeakError() {
    var callbacks = 0;
    chrome.tts.speak(
        'first try',
        {
         'enqueue': true,
         'onEvent': function(event) {
            chrome.test.assertEq('error', event.type);
            chrome.test.assertEq('epic fail', event.errorMessage);
            callbacks++;
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
    chrome.tts.speak(
        'second try',
        {
         'enqueue': true,
         'onEvent': function(event) {
            chrome.test.assertEq('end', event.type);
            callbacks++;
            if (callbacks == 2) {
              chrome.test.succeed();
            } else {
              chrome.test.fail();
            }
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  }
]);
