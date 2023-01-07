// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testSpeakWithOptionalArgs() {
    // This will fail.
    try {
      chrome.tts.speak();
      chrome.test.fail();
    } catch (e) {
    }

    // This will succeed but nothing will be spoken.
    chrome.tts.speak('');

    // This will succeed.
    chrome.tts.speak('Alpha');

    // This will fail.
    try {
      chrome.tts.speak(null);
      chrome.test.fail();
    } catch (e) {
    }

    // This will succeed.
    chrome.tts.speak('Bravo', {});

    // This will succeed.
    chrome.tts.speak('Charlie', null);

    // This will fail.
    try {
      chrome.tts.speak('Delta', 'foo');
      chrome.test.fail();
    } catch (e) {
    }

    // This will succeed.
    chrome.tts.speak('Echo', {}, function() {});

    // This will fail.
    try {
      chrome.tts.speak('Foxtrot', {}, 'foo');
      chrome.test.fail();
    } catch (e) {
    }

    chrome.test.succeed();
  }
]);
