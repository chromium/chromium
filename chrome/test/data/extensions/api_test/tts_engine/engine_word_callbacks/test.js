// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testWordCallbacks() {
    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function(utterance, options, sendTtsEvent) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('alpha beta gamma', utterance);
      sendTtsEvent({'type': 'word', 'charIndex': 0});
      sendTtsEvent({'type': 'word', 'charIndex': 6});
      sendTtsEvent({'type': 'word', 'charIndex': 11});
      sendTtsEvent({'type': 'end', 'charIndex': utterance.length});
    };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    var callbacks = 0;
    chrome.tts.speak(
        'alpha beta gamma',
        {
         'onEvent': function(event) {
           chrome.test.assertNoLastError();
           callbacks++;
           switch(callbacks) {
           case 1:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(0, event.charIndex);
             break;
           case 2:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(6, event.charIndex);
             break;
           case 3:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(11, event.charIndex);
             break;
           case 4:
             chrome.test.assertEq('end', event.type);
             chrome.test.assertEq(16, event.charIndex);
             chrome.test.succeed();
             break;
           default:
             chrome.test.fail();
           }
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  }
]);
