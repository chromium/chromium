// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Lacros Chrome.
// lacros_chrome_browsertests --gtest_filter="LacrosTtsApiTest.*"

chrome.test.runTests([
  function testGetVoices() {
    // We have to register listeners, or the voices provided
    // by this extension won't be returned.
    var speakListener = function (utterance, options, sendTtsEvent) {};
    var stopListener = function () {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);
    chrome.test.succeed();
  }
]);
