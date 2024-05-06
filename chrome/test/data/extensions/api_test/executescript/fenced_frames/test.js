// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var listenOnce = chrome.test.listenOnce;
var listenForever = chrome.test.listenForever;

// Keep track of the tab that we're running tests in, for simplicity.
var testTab = null;

var tests = [
  // Waits for fenced frame to load.
  function waitForFencedFrameLoad() {
    // We have to wait for the fenced frame to be loaded before we can
    // start the rest of the tests. The fenced frame sends a message once
    // it has been loaded.
    listenOnce(chrome.runtime.onMessage, function(message, sender) {
    });
  },

  // Tests that executeScript works in fenced frames.
  function executeScriptInAllFrames() {
    // allFrames == true should execute in fenced frames.
    chrome.tabs.executeScript(
        testTab.id,
        {code: 'window.location.pathname', allFrames: true, runAt:
         'document_idle'},
        (result) => {
          // Even though the document has 3 frames (the main frame and two
          // fenced frames), we only expect two frames to be included in the
          // injection, since one of the fenced frames is in its initial
          // un-navigated state. Regression test for
          // https://crbug.com/334991040.
          chrome.test.assertEq(2, result.length);
          chrome.test.assertTrue(result[0].endsWith('main.html'));
          chrome.test.assertTrue(result[1].endsWith('fenced_frame.html'));
          chrome.test.succeed();
        });
  },

  // Tests that executeScript works in main frames with child fenced frames.
  function executeScriptInMainFrame() {
    // allFrames == false should not execute in fenced fenced frames.
    chrome.tabs.executeScript(
        testTab.id,
        {code: 'window.location.pathname', allFrames: false, runAt:
         'document_idle'},
        (result) => {
          chrome.test.assertEq(1, result.length);
          chrome.test.assertTrue(result[0].endsWith('main.html'));
          chrome.test.succeed();
        });
  },
];

chrome.test.getConfig(async (config) => {
  var serverOrigin = `https://a.test:${config.testServer.port}`;
  var serverURL = serverOrigin + '/extensions/api_test/executescript/'
                           + 'fenced_frames/';
  const url = serverURL + 'main.html';

  testTab = await new Promise(function(resolve, reject) {
    chrome.tabs.create({url: url}, (value) => {
      resolve(value);
    });
  });

  chrome.test.runTests(tests);
});
