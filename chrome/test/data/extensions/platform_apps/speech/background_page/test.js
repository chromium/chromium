// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testWebkitSpeechRecognition() {
      var succeeded = false;

      var r = new webkitSpeechRecognition();
      r.onerror = function(e) {
        if (succeeded) {
          return;
        }
        chrome.test.fail();
      };
      r.onstart = function() {
        succeeded = true;
        chrome.test.succeed();
      };
      // With FakeSpeechRecognitionManager, we do not get onstart event. We
      // directly get results instead.
      r.onresult = function(e) {
        succeeded = true;
        chrome.test.assertTrue(e.results.length > 0);
        var transcript = e.results[0][0].transcript;
        chrome.test.assertEq('Pictures of the moon', transcript);
        chrome.test.succeed();
      };
      r.start();
    }
  ]);
});
