// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var listenOnce = chrome.test.listenOnce;
var listenForever = chrome.test.listenForever;

var allTests = [
  function waitForFencedFrameLoad() {
    // We have to wait for the fenced frame to be loaded before we can
    // start the rest of the tests. The fenced frame sends a message
    // once it has been loaded.
    listenOnce(chrome.runtime.onMessage, function(message, sender) {});
  },

  function testFocusInIframes() {
    chrome.automation.getDesktop(function(rootNode) {
      // Poll until we get the inner button, which is in the inner
      // fenced frame.
      const id = setInterval(() => {
        var innerButton =
            rootNode.find({attributes: {name: 'Inner'}, role: 'button'});
        if (innerButton) {
          clearInterval(id);
          chrome.test.succeed()
        }
      }, 100);
    });
  }
];

chrome.test.getConfig(async (config) => {
  var url = 'http://localhost:' + config.testServer.port + '/fencedframe/fencedframe_outer.html';

  testTab = await new Promise(function(resolve, reject) {
    chrome.tabs.create({url: url}, (value) => {
      resolve(value);
    });
  });

  chrome.test.runTests(allTests);
});
