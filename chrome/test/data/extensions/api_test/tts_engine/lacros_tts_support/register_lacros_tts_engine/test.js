// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.SpeakAshUtteranceWithLacrosSpeechEngine
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome

chrome.test.runTests([
  // Test speaking an Lacros utterance with a voice provided by a speech engine
  // extension registered from ash.
  function testSpeakLacrosUtteranceWithAshTtsEngine() {
    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Hello from Ash', utterance);
      sendTtsEvent({
        'type': 'end',
        'charIndex': utterance.length
      });
    };
    var stopListener = function () {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);
    chrome.test.succeed();
  }
]);
