// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests.exe --gtest_filter="TtsApiTest.*"

var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function testNetworkSpeechVoices() {
    chrome.tts.getVoices(pass(function(voices) {
      chrome.test.assertTrue(voices.length >= 19);
      for (var i = 0; i < voices.length; i++) {
        chrome.test.assertEq(true, voices[i].remote);
      }
    }));
  }
]);
