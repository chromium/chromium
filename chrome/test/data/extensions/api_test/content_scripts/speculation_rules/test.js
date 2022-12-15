// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testServerPort = 0;

function getUrl(path) {
  return `http://default.test:${testServerPort}/${path}`;
}

async function testInjectSpeculationRules() {
  // Flags to check if the page is loaded only once.
  let testPageLoaded = false;
  let prerenderPageLoaded = false;

  // Monitor messages from the injected content_script.js.
  const testCallback = (message, sender, sendResponse) => {
    if (message == getUrl('empty.html')) {
      // empty.html is loaded.
      chrome.test.assertFalse(testPageLoaded);
      testPageLoaded = true;
    } else if (message == getUrl('title1.html')) {
      // title1.html is prerendered as a result of the permitted inline
      // speculation rules injection.
      chrome.test.assertFalse(prerenderPageLoaded);
      chrome.test.assertTrue(testPageLoaded);
      chrome.test.succeed();
    } else {
      chrome.test.fail('Unexpected message: ' + message);
    }
  };
  chrome.runtime.onMessage.addListener(testCallback);

  // Load the initial page that runs the content_script.js to inject a
  // speculation rules.
  chrome.tabs.update({ url: getUrl('empty.html') });
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;
  chrome.test.runTests([
    testInjectSpeculationRules,
  ]);
})
