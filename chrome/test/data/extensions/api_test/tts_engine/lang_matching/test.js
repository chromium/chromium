// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testWordCallbacks() {
    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function(utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      sendTtsEvent({'type': 'end', 'charIndex': utterance.length});
    };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // Make sure that a lang of 'fr-FR' goes to our engine,
    // even though the engine only registered 'fr'.
    chrome.tts.speak(
        'dummy utterance',
        {
         'lang': 'fr-FR',
         'onEvent': function(event) {
           chrome.test.assertNoLastError();
           chrome.test.succeed();
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  }
]);
