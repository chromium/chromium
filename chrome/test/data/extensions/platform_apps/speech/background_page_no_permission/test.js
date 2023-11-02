// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testWebkitSpeechRecognition() {
      var r = new webkitSpeechRecognition();
      r.onerror = function(e) {  // Expect to fail.
        chrome.test.assertEq('not-allowed', e.error);
        chrome.test.succeed();
      };
      r.onstart = function() {
        chrome.test.fail();
      };
      r.start();
    }
  ]);
});
