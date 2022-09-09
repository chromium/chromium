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
    var request = new XMLHttpRequest();
    request.addEventListener("load", requestListener);
    request.open("GET", test_dir);
    request.send();
  },

  function testFile() {
    var request = new XMLHttpRequest();
    request.addEventListener("load", requestListener);
    request.open("GET", test_dir + "/empty.html");
    request.send();
  }
];

chrome.test.getConfig(function(config) {
  test_dir = "file://" + config.customArg;
  chrome.test.runTests(tests);
})
