// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testWordCallbacks() {
    var callbacks = 0;
    chrome.tts.speak(
        'one two three',
        {
         'onEvent': function(event) {
           callbacks++;
           switch(callbacks) {
           case 1:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(0, event.charIndex);
             break;
           case 2:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(4, event.charIndex);
             break;
           case 3:
             chrome.test.assertEq('word', event.type);
             chrome.test.assertEq(8, event.charIndex);
             break;
           case 4:
             chrome.test.assertEq('end', event.type);
             chrome.test.assertEq(13, event.charIndex);
             chrome.test.succeed();
             break;
           default:
             chrome.test.fail();
           }
         }
        });
  }
]);
