// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from ash with lacros(lacros_tts_support enabled).
// browser_tests
//     --lacros-chrome-path={your_build_path}/lacros_clang_x64/test_lacros_chrome
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
  },
  // Test speaking an utterance issued from an ash extension with a voice
  // provided by a speech engine registered in ash.
  function testSpeakAshUtteranceWithAshTtsEngine() {
    var calledOurEngine = false;

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('ash extension speech', utterance);
      calledOurEngine = true;
      sendTtsEvent({
        'type': 'end',
        'charIndex': utterance.length
      });
    };
    var stopListener = function () {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // This call should go to our own speech engine loaded in ash.
    chrome.tts.speak(
      'ash extension speech', {
        'voiceName': 'Amy',
        'onEvent': function (event) {
          if (event.type == 'end') {
            chrome.test.assertEq(true, calledOurEngine);
            chrome.ttsEngine.onSpeak.removeListener(speakListener);
            chrome.ttsEngine.onStop.removeListener(stopListener);
            chrome.test.succeed();
          }
        }
      },
      function () {
        chrome.test.assertNoLastError();
      });
  }
]);
