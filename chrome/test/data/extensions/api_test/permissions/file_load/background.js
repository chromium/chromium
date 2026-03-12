// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

var test_dir;

function requestListener() {
  chrome.test.succeed();
}

var tests = [
  function testDirectoryListing() {
    fetch(test_dir).then(requestListener).catch(function(err) {
      chrome.test.fail(err.toString());
    });
  },

  function testFile() {
    fetch(test_dir + "/empty.html").then(requestListener).catch(function(err) {
      chrome.test.fail(err.toString());
    });
  }
];

chrome.test.getConfig(function(config) {
  test_dir = "file://" + config.customArg;
  chrome.test.runTests(tests);
})
