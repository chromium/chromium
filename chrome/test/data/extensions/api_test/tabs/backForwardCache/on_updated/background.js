// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedEventData;
var capturedEventData;
var shouldIgnore = true;

function expect(data) {
  chrome.tabs.onUpdated.addListener(function(tabId, info, tab) {
    // Wait until the first loading of a non-blank url.
    if (info.status === 'loading' && info.url != 'about:blank')
      shouldIgnore = false;
    if (shouldIgnore)
      return;
    capturedEventData.push(info);
    checkExpectations();
  });

  expectedEventData = data;
  capturedEventData = [];
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  chrome.test.assertEq(JSON.stringify(expectedEventData),
      JSON.stringify(capturedEventData));
  chrome.test.succeed();
}

// Helper function. Turns a function returning an object in a callback into a
// promise. It helps keeping the code at the same indentation level.
function promise(fun, ...args) {
  return new Promise(function(resolve, reject) {
    fun(...args, function(value) {
      resolve(value);
    });
  });
}

chrome.test.getConfig(async function(config) {
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let port = config.testServer.port;
  let URL_A = "http://a.com:" + port +
      "/extensions/api_test/tabs/backForwardCache/on_updated/a.html";
  let URL_B = "http://b.com:" + port +
        "/extensions/api_test/tabs/backForwardCache/on_updated/b.html";

  chrome.test.runTests([
    function backForwardNavigation() {
      expect([
        { status: 'loading', url: URL_A },
        { status: 'complete' },
        { status: 'loading', url: URL_B },

        // Asserts that back forward cache restoring A generates loading
        // and complete events.
        { status: 'loading', url: URL_A },
        { status: 'complete' },
      ]);

      chrome.tabs.update(tab.id, { url: URL_A });
    }
  ])
});
