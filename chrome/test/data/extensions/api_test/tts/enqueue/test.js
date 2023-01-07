// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testEnqueue() {
    var callbacks = 0;
    chrome.tts.speak(
        'text 1',
        {
         'enqueue': true,
         'onEvent': function(event) {
           chrome.test.assertEq('end', event.type);
           callbacks++;
         }
        },
        function() {
          chrome.test.assertNoLastError();
          callbacks++;
        });
    chrome.tts.speak(
        'text 2',
        {
         'enqueue': true,
         'onEvent': function(event) {
           chrome.test.assertEq('end', event.type);
           callbacks++;
           if (callbacks == 4) {
             chrome.test.succeed();
           } else {
             chrome.test.fail();
           }
         }
        },
        function() {
          chrome.test.assertNoLastError();
          callbacks++;
        });
  }
]);
