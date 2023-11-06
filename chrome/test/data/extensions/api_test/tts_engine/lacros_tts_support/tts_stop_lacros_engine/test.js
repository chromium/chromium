// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.StopLacrosUtteranceWithLacrosTtsEngine
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome

chrome.test.runTests([
  // Test tts.stop called from a Lacros extension to stop a Lacros utterance
  // spoken by a voice provided by a speech engine extension registered from
  // Lacros.
  function testStopLacrosUtteranceWithLacrosTtsEngine() {
    let lacrosEngineSpeaking = false;
    let stopLacrosEngineCalled = false;
    let firstUtteranceInterrupted = false;
    let secondUtteranceCancelled = false;

    function testSucceedIfTtsStopDone() {
      // When tts.Stop is processed by Ash's TtsController, the controller
      // stops the current utterance if it matches source_url passed from
      // tts.stop by sending onStop event to the speech engine which is speaking
      // the current utterance, and an 'interrupt' event to the current
      // utterance; it also clears the utterance queue by sending 'cancel' event
      // to all the queued utterances.
      if (stopLacrosEngineCalled && firstUtteranceInterrupted &&
        secondUtteranceCancelled) {
        chrome.ttsEngine.onSpeak.removeListener(speakListener);
        chrome.ttsEngine.onStop.removeListener(stopListener);
        chrome.test.succeed();
      }
    }

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    let speakListener = function (utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(lacrosEngineSpeaking, false);
      chrome.test.assertEq(stopLacrosEngineCalled, false);
      chrome.test.assertEq('Speak the first utterance', utterance);
      lacrosEngineSpeaking = true;
      sendTtsEvent({
        'type': 'start',
        'charIndex': utterance.length
      });
    };
    let stopListener = function () {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(lacrosEngineSpeaking, true);
      chrome.test.assertEq(stopLacrosEngineCalled, false);
      stopLacrosEngineCalled = true;
      testSucceedIfTtsStopDone();
    };
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // Call tts.speak to speak the first utterance. The utterance will be spoken
    // by our own speech engine running in Lacros.
    chrome.tts.speak(
      'Speak the first utterance', {
        'voiceName': 'Alice',
        'onEvent': function (event) {
          if (event.type == 'start') {
            chrome.test.assertEq(true, lacrosEngineSpeaking);
            chrome.test.assertEq(false, stopLacrosEngineCalled);
            // Call tts.stop after Lacros speech engine started speaking
            // the first utterance.
            chrome.tts.stop();
          } else if (event.type == 'interrupted') {
            firstUtteranceInterrupted = true;
            testSucceedIfTtsStopDone();
          }
        }
      },
      function () {
        chrome.test.assertNoLastError();
      });

    // Request to speak the second utterance. This utterance should be queued
    // by Ash's TtsController since the first utterance has not finished at
    // this point. The utterance should be cancelled later when tts.stop is
    // called.
    chrome.tts.speak(
      'Speak the second utterance', {
        'voiceName': 'Alice',
        'enqueue': true,
        'onEvent': function (event) {
          chrome.test.assertEq(event.type, 'cancelled');
          secondUtteranceCancelled = true;
          testSucceedIfTtsStopDone();
        }
      },
      function () {
        chrome.test.assertNoLastError();
      });
  }
]);
