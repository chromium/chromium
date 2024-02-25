// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.PauseBeforeSpeakWithLacrosTtsEngine
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome

chrome.test.runTests([
  async function testPauseBeforeSpeakWithLacrosTtsEngine() {
    const utteranceText = "Hello";

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    let speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(utteranceText, utterance);
      sendTtsEvent({
        'type': 'end',
        'charIndex': utterance.length
      });
    };
    chrome.ttsEngine.onSpeak.addListener(speakListener);

    let stopListener = function () {};
    chrome.ttsEngine.onStop.addListener(stopListener);

    // Pause speech synthesis before calling tts.speak.
    chrome.tts.pause();

    // Request to speak an utterance with Lacros speech engine. The utterance
    // should be queued by Ash's TtsController.
    let ttsSpeakPromise = new Promise((resolve) => {
      chrome.tts.speak(
        utteranceText, {
          'voiceName': 'Alice',
          'onEvent': function (event) {
            if (event.type == 'end') {
              resolve();
            }
          }
        },
        function () {
          chrome.test.assertNoLastError();
        });
    });

    // Resume speech synthesis. This should enable the Ash TtsController to send
    // the queued utterance to Lacros speech engine.
    chrome.tts.resume();

    await ttsSpeakPromise.then(() => {
      chrome.ttsEngine.onSpeak.removeListener(speakListener);
      chrome.ttsEngine.onStop.removeListener(stopListener);
      chrome.test.succeed();
    });
  }
]);
