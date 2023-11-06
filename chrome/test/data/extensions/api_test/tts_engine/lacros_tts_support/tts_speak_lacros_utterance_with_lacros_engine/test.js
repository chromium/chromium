// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.SpeakLacrosUtteranceWithLacrosTtsEngine
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome

chrome.test.runTests([
  // Test speaking an Lacros utterance with a voice provided by a speech engine
  // extension registered from Lacros.
  function testSpeakLacrosUtteranceWithLacrosTtsEngine() {
    var calledLacrosEngine = false;
    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Lacros extension speech', utterance);
      calledLacrosEngine = true;
      sendTtsEvent({
        'type': 'end',
        'charIndex': utterance.length
      });
    };
    var stopListener = function () {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // This call should go to our own speech engine running in Lacros.
    chrome.tts.speak(
      'Lacros extension speech', {
        'voiceName': 'Alice',
        'onEvent': function (event) {
          if (event.type == 'end') {
            chrome.test.assertEq(true, calledLacrosEngine);
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
