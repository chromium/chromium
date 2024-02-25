// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.PauseDuringSpeakWithLacrosTtsEngine
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome

chrome.test.runTests([
  function testPauseDuringSpeakWithLacrosTtsEngine() {
    let sendTtsEventFunc;
    const utteranceText = "Hello";

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    let onSpeakPromise = new Promise((resolve) => {
      chrome.ttsEngine.onSpeak.addListener(function listener(
        utterance, options, sendTtsEvent) {
        chrome.test.assertNoLastError();
        sendTtsEventFunc = sendTtsEvent;
        chrome.test.assertEq(utteranceText, utterance);
        sendTtsEvent({
          'type': 'start',
          'charIndex': utterance.length
        });
        chrome.ttsEngine.onSpeak.removeListener(listener);
        resolve();
      });
    });

    let stopListener = function () {};
    chrome.ttsEngine.onStop.addListener(stopListener);

    let onPausePromise = new Promise((resolve) => {
      chrome.ttsEngine.onPause.addListener(function listener() {
        chrome.test.assertNoLastError();
        chrome.ttsEngine.onPause.removeListener(listener);
        resolve();
      })
    });

    let onResumePromise = new Promise((resolve) => {
      chrome.ttsEngine.onResume.addListener(function listener() {
        chrome.test.assertNoLastError();
        // Simulate the speech engine resuming speech synthesis and finish
        // the utterance.
        sendTtsEventFunc({
          'type': 'end',
          'charIndex': utteranceText.length
        });
        chrome.ttsEngine.onResume.removeListener(listener);
        resolve();
      })
    });

    // Call tts.speak to speak an utterance with our own speech engine running
    // in Lacros.
    let ttsSpeakPromise = new Promise((resolve) => {
      let startEventReceived = false;
      chrome.tts.speak(
        utteranceText, {
          'voiceName': 'Alice',
          'onEvent': function (event) {
            if (event.type == 'start') {
              startEventReceived = true;
            } else if (event.type == 'end') {
              chrome.test.assertEq(startEventReceived, true);
              resolve();
            }
          }
        },
        function () {
          chrome.test.assertNoLastError();
        });
    });

    // Request pausing speech synthesis during the Lacros speech engine speaking
    // an utterance. Ash's TtsController will pause speech synthesis and send
    // onPause event to the Lacros speech engine speaking the current utterance.
    chrome.tts.pause();

    // Request resuming speech synthesis. Ash's TtsController will resume speech
    // synthesis and send onResume event to the Lacros speech engine it has
    // paused previously.
    chrome.tts.resume();

    // Verify that all ttsEngine events have been delivered to the speech
    // engine, and tts.speak finishes the utterance.
    Promise.all([onSpeakPromise, onPausePromise, onResumePromise,
      ttsSpeakPromise
    ]).then(() => {
      chrome.ttsEngine.onStop.removeListener(stopListener);
      chrome.test.succeed();
    });
  }
]);
