// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from ash with lacros..
// browser_tests --lacros-chrome-path={your_build_path}/lacros_clang_x64
//     --gtest_filter="AshTtsApiTest.*"

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
