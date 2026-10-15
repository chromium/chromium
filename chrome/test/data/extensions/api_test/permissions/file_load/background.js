// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

let testDir;

function requestListener() {
  chrome.test.succeed();
}

const tests = [
  function testDirectoryListing() {
    fetch(testDir).then(requestListener).catch(function(err) {
      chrome.test.fail(err.toString());
    });
  },

  function testFile() {
    fetch(`${testDir}/empty.html`).then(requestListener).catch(function(err) {
      chrome.test.fail(err.toString());
    });
  },
];

chrome.test.getConfig(function(config) {
  testDir = `file://${config.customArg}`;
  // The renderer only allows this extension to load file: URLs after the
  // extension has been activated in the renderer process. Activation happens
  // asynchronously on the renderer main thread while this service worker
  // script runs on a worker thread, so wait for the C++ side of the test to
  // signal that activation is complete.
  chrome.test.sendMessage('ready', function() {
    chrome.test.runTests(tests);
  });
});
