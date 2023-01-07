// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testTtsEngineError() {
    // Register listeners for speech functions, but have speak return an
    // error when it's used.
    var speakListener = function(utterance, options, sendTtsEvent) {
      chrome.test.assertEq('extension speech', utterance);

      try {
        // This should fail because 'foo' isn't a valid event type.
        sendTtsEvent({'type': 'foo'});
        chrome.test.fail();
      } catch (e) {
      }

      try {
        // This should fail: charIndex should be an integer.
        sendTtsEvent(
            {type: 'error', charIndex: 0.1, errorMessage: 'some error'});
        chrome.test.fail();
      } catch (e) {}

      // This won't actually send an event, and an error will be logged
      // to the console, because we haven't registered the 'word'
      // event type in our manifest.
      sendTtsEvent({'type': 'word'});

      // This will succeed.
      sendTtsEvent({
        'type': 'error',
        'charIndex': 0,
        'errorMessage': 'extension tts error'});
    };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // This should go to our own TTS engine, and we can check that we
    // get the error message.
    chrome.tts.speak(
        'extension speech',
        {
         'onEvent': function(event) {
           chrome.test.assertEq('error', event.type);
           chrome.test.assertEq('extension tts error', event.errorMessage);
           chrome.ttsEngine.onSpeak.removeListener(speakListener);
           chrome.ttsEngine.onStop.removeListener(stopListener);
           chrome.test.succeed();
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  }
]);
