// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test running from Lacros with Ash.
// build/lacros/test_runner.py test
//     {path_to_lacros_build}/lacros_chrome_browsertests
//     --gtest_filter=LacrosTtsApiTest.IsSpeaking
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome
// and TTS api test running from Ash with Lacros.
// browser_tests
//     --lacros-chrome-path=out_linux_ash/Release/lacros_clang_x64/test_lacros_chrome
//     --gtest_filter=AshTtsApiTest.IsSpeaking

chrome.test.runTests([
  async function testIsSpeaking() {
    let sendTtsEventFunc;
    const utteranceText = "Hello";

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    let speakListener = function (utterance, options, sendTtsEvent) {
      sendTtsEventFunc = sendTtsEvent;
      chrome.test.assertEq(utteranceText, utterance);
      sendTtsEvent({
        'type': 'start',
        'charIndex': utterance.length
      });
    };
    chrome.ttsEngine.onSpeak.addListener(speakListener);

    let stopListener = function () {};
    chrome.ttsEngine.onStop.addListener(stopListener);

    // Before speaking.
    await chrome.tts.isSpeaking((speaking) => {
      chrome.test.assertEq(speaking, false);
    });

    // Call tts.speak to speak an utterance with our own speech engine.
    let ttsSpeakPromise = new Promise((resolve) => {
      chrome.tts.speak(
        utteranceText, {
          'voiceName': 'Alice',
          'onEvent': function (event) {
            if (event.type == 'start') {
              // During speaking.
              chrome.tts.isSpeaking((speaking) => {
                chrome.test.assertEq(speaking, true);
                sendTtsEventFunc({
                  'type': 'end',
                  'charIndex': utteranceText.length
                });
              });
            } else if (event.type == 'end') {
              // After speaking.
              chrome.tts.isSpeaking((speaking) => {
                chrome.test.assertEq(speaking, false);
                resolve();
              });
            }
          }
        },
        function () {
          chrome.test.assertNoLastError();
        });
    });

    await ttsSpeakPromise.then(() => {
      chrome.ttsEngine.onSpeak.removeListener(speakListener);
      chrome.ttsEngine.onStop.removeListener(stopListener);
      chrome.test.succeed();
    });
  }
]);
