// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testQueueInterrupt() {
    // In this test, two utterances are queued, and then a third
    // interrupts. The first gets interrupted, the second never gets spoken
    // at all. The test expectations in tts_extension_apitest.cc ensure that
    // the first call to tts.speak keeps going until it's interrupted.
    var callbacks = 0;
    chrome.tts.speak(
        'text 1',
        {
         'enqueue': true,
         'onEvent': function(event) {
           chrome.test.assertEq('interrupted', event.type);
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
           chrome.test.assertEq('cancelled', event.type);
           callbacks++;
         }
        }, function() {
          chrome.test.assertNoLastError();
          callbacks++;
        });
    chrome.tts.speak(
        'text 3',
        {
         'enqueue': false,
         'onEvent': function(event) {
           chrome.test.assertEq('end', event.type);
           callbacks++;
           if (callbacks == 6) {
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
