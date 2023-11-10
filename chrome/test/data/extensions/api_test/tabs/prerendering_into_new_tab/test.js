// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testServerPort = 0;
let tabId = 0;
let prerenderingTabId = 0;
let prerenderingFrameId = 0;
let prerenderingDocumentId = 0;

function getUrl(path) {
  return `http://example.com:${
      testServerPort}/extensions/api_test/tabs/prerendering_into_new_tab/${
      path}`;
}

async function setup() {
  // The pre-rendered frame includes a resource for empty.js. We use a
  // webRequest listener to intercept the call for the resource in order to
  // identify the frame / document of the prerendered page.
  const kInitiatorUrl = getUrl('initiator.html');
  const kEmptyJsUrl = getUrl('empty.js');
  await new Promise(resolve => {
    const onBeforeRequest =
        (details => {
          if (details.documentLifecycle === 'prerender') {
            prerenderingTabId = details.tabId;
            prerenderingFrameId = details.frameId;
            prerenderingDocumentId = details.documentId;
            chrome.test.assertNe(0, prerenderingFrameId);
            chrome.webRequest.onBeforeRequest.removeListener(onBeforeRequest);
            resolve();
          }
        });
    chrome.webRequest.onBeforeRequest.addListener(
        onBeforeRequest, {urls: [kEmptyJsUrl]}, []);
    chrome.test.waitForRoundTrip('msg', () => {
      chrome.tabs.update(tabId, {url: kInitiatorUrl});
    });
  });
  chrome.test.assertNe(0, prerenderingDocumentId);
}

// Checks that `allFrames: true` does not return an invisible tab (pre-rendered
// new tab frames).
async function testGetTitleForAllFrames() {
  await setup();
  chrome.tabs.executeScript(
      prerenderingTabId, {allFrames: true, code: 'document.title;'},
      results => {
        chrome.test.assertEq(undefined, results);
      });
  chrome.tabs.executeScript(
      tabId, {allFrames: true, code: 'document.title;'}, results => {
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('initiator', results[0]);
        chrome.test.succeed();
      });
}

// Checks that `getAllInWindow` does not return an invisible tab (pre-rendered
// new tab frames).
async function testGetAllInWindow() {
  await setup();
  console.log('testGetAllInWindow called');
  chrome.tabs.getAllInWindow(null, function(tabs) {
    chrome.test.assertEq(1, tabs.length);
    chrome.test.assertEq(getUrl('initiator.html'), tabs[0].url);
    chrome.test.succeed();
  });
}

// Checks that `query` does not return an invisible tab (pre-rendered
// new tab frames).
async function testQuery() {
  await setup();
  chrome.tabs.query({}, function(tabs) {
    chrome.test.assertEq(1, tabs.length);
    chrome.test.assertEq(getUrl('initiator.html'), tabs[0].url);
    chrome.test.succeed();
  });
}

// Checks that `frameId` not returning an invisible tab when specifying a
// pre-rendered frame.
async function testGetTitleByFrameId() {
  await setup();
  chrome.tabs.executeScript(
      prerenderingTabId,
      {frameId: prerenderingFrameId, code: 'document.title;'}, results => {
        chrome.test.assertEq(undefined, results);
        chrome.test.succeed();
      });
}

// Checks that manifest v2 does not support `documentId`.
async function testGetTitleByDocumentId() {
  await setup();
  chrome.test.assertThrows(
      chrome.tabs.executeScript,
      [
        prerenderingTabId,
        {documentId: prerenderingDocumentId, code: 'document.title;'},
        results => chrome.test.fail('should not succeed.')
      ],
      'Error in invocation of tabs.executeScript(optional integer tabId, ' +
          'extensionTypes.InjectDetails details, optional function ' +
          'callback): Error at parameter \'details\': Unexpected property: \'' +
          'documentId\'.');
  chrome.test.succeed();
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;
  chrome.test.assertNe(0, testServerPort);

  const tabs = await new Promise(
      resolve => chrome.tabs.query({active: true}, tabs => resolve(tabs)));
  chrome.test.assertEq(1, tabs.length);
  tabId = tabs[0].id;

  // TODO(https://crbug.com/1350676): add more tests for tabs.on* event listeners.
  chrome.test.runTests([
    testGetTitleForAllFrames,
    testGetAllInWindow,
    testQuery,
    testGetTitleByFrameId,
    testGetTitleByDocumentId,
  ]);
});
