// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from ash with lacros.
// browser_tests
//     --lacros-chrome-path={your_build_path}/lacros_clang_x64/test_lacros_chrome
//     --gtest_filter="AshTtsApiTest.SpeakLacrosUtteranceWithAshSpeechEngine"

chrome.test.runTests([
  // Test speaking an Lacros utterance with a voice provided by a speech engine
  // extension registered from ash.
  function testSpeakLacrosUtteranceWithAshTtsEngine() {
    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Hello from Lacros', utterance);
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
