// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testNoListeners() {
    // This call should go to native speech because we haven't registered
    // any listeners.
    chrome.tts.speak(
        'native speech',
        {
         'onEvent': function(event) {
           if (event.type == 'end') {
             chrome.test.succeed();
           }
         }
        }, function() {
          chrome.test.assertNoLastError();
        });
  },
  function testTtsEngine() {
    var calledOurEngine = false;

    // Register listeners for speech functions, enabling this extension
    // to be a TTS engine.
    var speakListener = function(utterance, options, sendTtsEvent) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('extension speech', utterance);
        calledOurEngine = true;
        sendTtsEvent({'type': 'end', 'charIndex': utterance.length});
      };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // This call should go to our own speech engine.
    chrome.tts.speak(
        'extension speech',
        {
         'voiceName': 'Alice',
         'onEvent': function(event) {
           if (event.type == 'end') {
             chrome.test.assertEq(true, calledOurEngine);
             chrome.ttsEngine.onSpeak.removeListener(speakListener);
             chrome.ttsEngine.onStop.removeListener(stopListener);
             chrome.test.succeed();
           }
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  },
  function testVoiceMatching() {
    // Count the number of times our callback functions have been called.
    var callbacks = 0;
    // Count the number of times our TTS engine has been called.
    var speakListenerCalls = 0;

    // Register listeners for speech functions.
    var speakListener = function(utterance, options, sendTtsEvent) {
      speakListenerCalls++;
      sendTtsEvent({'type': 'end', 'charIndex': utterance.length});
    };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    // These don't match the voices in the manifest, so they should
    // go to native speech. The gmock assertions in TtsApiTest::RegisterEngine
    // enforce that the native TTS handlers are called.
    chrome.tts.speak(
        'native speech 2',
        {
         'voiceName': 'George',
         'enqueue': true,
         'onEvent': function(event) {
           if (event.type == 'end') {
             callbacks++;
           }
         }
        }, function() {
          chrome.test.assertNoLastError();
        });
    chrome.tts.speak(
        'native speech 3',
        {
         'voiceName': 'French',
         'lang': 'fr-FR',
         'enqueue': true,
         'onEvent': function(event) {
           if (event.type == 'end') {
             callbacks++;
           }
         }
        }, function() {
          chrome.test.assertNoLastError();
        });

    // These do match the voices in the manifest, so they should go to our
    // own TTS engine.
    chrome.tts.speak(
        'extension speech 2',
        {
         'voiceName': 'Alice',
         'enqueue': true,
         'onEvent': function(event) {
           if (event.type == 'end') {
             callbacks++;
           }
         }
        }, function() {
          chrome.test.assertNoLastError();
        });
    chrome.tts.speak(
        'extension speech 3',
        {
         'voiceName': 'Pat',
         'enqueue': true,
         'onEvent': function(event) {
           if (event.type == 'end') {
             callbacks++;
             chrome.ttsEngine.onSpeak.removeListener(speakListener);
             chrome.ttsEngine.onStop.removeListener(stopListener);
             if (callbacks == 4 && speakListenerCalls == 2) {
               chrome.test.succeed();
             }
           }
         }
        }, function() {
          chrome.test.assertNoLastError();
        });
  },
  function testGetVoices() {
    // We have to register listeners, or the voices provided
    // by this extension won't be returned.
    var speakListener = function(utterance, options, sendTtsEvent) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('extension speech', utterance);
        sendTtsEvent({'type': 'end', 'charIndex': utterance.length});
      };
    var stopListener = function() {};
    chrome.ttsEngine.onSpeak.addListener(speakListener);
    chrome.ttsEngine.onStop.addListener(stopListener);

    chrome.tts.getVoices(function(voices) {
      chrome.test.assertEq(4, voices.length);

      chrome.test.assertEq('Alice', voices[0].voiceName);
      chrome.test.assertEq('en-US', voices[0].lang);

      chrome.test.assertEq('Pat', voices[1].voiceName);
      chrome.test.assertEq('en-US', voices[1].lang);

      chrome.test.assertEq('Cat', voices[2].voiceName);

      chrome.test.assertEq('TestNativeVoice', voices[3].voiceName);
      chrome.test.assertEq('en-GB', voices[3].lang);

      chrome.test.assertEq(voices[0].gender, undefined);
      chrome.test.assertEq(voices[1].gender, undefined);
      chrome.test.assertEq(voices[2].gender, undefined);

      chrome.test.succeed();
    });
  }
]);
