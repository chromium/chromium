// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testPauseBeforeSpeak() {
    chrome.tts.pause();
    chrome.tts.speak(
        'test 1',
        {
         'enqueue': true,
         'onEvent': function(event) {
           if (event.type == 'end')
             chrome.test.succeed();
         }
        });
    chrome.tts.resume();
  },
  function testPauseDuringSpeak() {
    chrome.tts.speak(
        'test 2',
        {
         'onEvent': function(event) {
           if (event.type == 'end')
             chrome.test.succeed();
         }
        });
    chrome.tts.pause();
    chrome.tts.resume();
  }
]);
