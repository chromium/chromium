// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS engine api test. This test doesn't actually exercise the API at all,
// it just makes sure that the ttsEngine permission is sufficient to make use
// of the API.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testTtsEngineApiSucceeds() {
    var speakListener = function(utterance, options, sendTtsEvent) {
      sendTtsEvent({'type': 'end'});
    };
    var stopListener = function() {};

    // This regressed after a recent refactoring because the internal
    // bindings for chrome.ttsEngine.onSpeak.addListener reference
    // chrome.tts.onEvent, which wasn't included by the ttsEngine permission.
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  }
]);
