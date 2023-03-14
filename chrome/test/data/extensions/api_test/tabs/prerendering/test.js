// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testServerPort = 0;
let tabId = 0;
let prerenderingFrameId = 0;
let prerenderingDocumentId = 0;

function getUrl(path) {
  return `http://example.com:${
      testServerPort}/extensions/api_test/tabs/prerendering/${path}`;
}

async function setup() {
  // The pre-rendered frame includes a resource for empty.js. We use a
  // webRequest listener to intercept the call for the resource in order to
  // identify the frame / document of the prerendered page.
  const kInitiatorUrl = getUrl('initiator.html');
  const kEmptyJsUrl = getUrl('empty.js');
  await new Promise(resolve => {
    const onBeforeRequest =
        ((resolve, details) => {
          prerenderingFrameId = details.frameId;
          prerenderingDocumentId = details.documentId;
          chrome.test.assertNe(0, prerenderingFrameId);
          chrome.webRequest.onBeforeRequest.removeListener(onBeforeRequest);
          resolve();
        }).bind(this, resolve);
    chrome.webRequest.onBeforeRequest.addListener(
        onBeforeRequest, {urls: [kEmptyJsUrl]}, []);
    chrome.test.waitForRoundTrip('msg', () => {
      chrome.tabs.update(tabId, {url: kInitiatorUrl});
    });
  });
  chrome.test.assertNe(0, prerenderingDocumentId);
}

// Checks if `allFrames: true` doesn't order to include pre-rendered frames.
async function testGetTitleForAllFrames() {
  await setup();
  chrome.tabs.executeScript(
      tabId, {allFrames: true, code: 'document.title;'}, results => {
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('initiator', results[0]);
        chrome.test.succeed();
      });
}

// Checks if `frameId` works to specify a pre-rendered frame.
async function testGetTitleByFrameId() {
  await setup();
  chrome.tabs.executeScript(
      tabId, {frameId: prerenderingFrameId, code: 'document.title;'},
      results => {
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('prerendering', results[0]);
        chrome.test.succeed();
      });
}

// Checks if manifest v2 doesn't support `documentId`.
async function testGetTitleByDocumentId() {
  await setup();
  chrome.test.assertThrows(
      chrome.tabs.executeScript,
      [
        tabId, {documentId: prerenderingDocumentId, code: 'document.title;'},
        results => chrome.test.fail('should not succeed.')
      ],
      'Error in invocation of tabs.executeScript(optional integer tabId, ' +
          'extensionTypes.InjectDetails details, optional function ' +
          'callback): Error at parameter \'details\': Unexpected property: \'' +
          'documentId\'.');
  chrome.test.succeed();
}

// Checks if injected scripts continue working while activating the pre-rendered
// pages. Also, checks if content scripts can activate the pre-rendered page.
async function testActivateOnExecution() {
  await setup();
  await new Promise(resolve => {
    // Inject a script that keeps alive until the page activation.
    chrome.tabs.executeScript(
        tabId, {
          frameId: prerenderingFrameId,
          code: `document.addEventListener('prerenderingchange', () => {
               document.title = 'activated';
             });`
        },
        result => {
          // No results, but just checks if it doesn't crash.
          resolve();
        });
  });

  // Start monitoring tab update events.
  chrome.tabs.onUpdated.addListener(function cb(updatedTabId, changeInfo, tab) {
    chrome.test.assertEq(tabId, updatedTabId);
    if (changeInfo.title && changeInfo.title === 'activated') {
      chrome.tabs.onUpdated.removeListener(cb);
      chrome.test.succeed();
    }
  });

  // Inject a script that activates the pre-rendered page.
  chrome.tabs.executeScript(
      tabId, {code: 'window.location.href = "./prerendering.html";'});
}

async function testExecuteAfterActivation() {
  await setup();

  // Start monitoring tab update events.
  chrome.tabs.onUpdated.addListener(function cb(updatedTabId, changeInfo, tab) {
    chrome.test.assertEq(tabId, updatedTabId);
    if (changeInfo.status && changeInfo.status === 'complete') {
      // Specify the hidden FrameTreeNodeId for the activated frame. JavaScript
      // could not know it, but it should just work as 0 is just an alternative
      // and internal FrameTreeNodeId should also work like a frameId.
      chrome.tabs.executeScript(
        tabId, { frameId: 1, code: 'document.title' },
        results => {
          chrome.tabs.onUpdated.removeListener(cb);
          chrome.test.assertEq(1, results.length);
          chrome.test.assertEq('prerendering', results[0]);
          chrome.test.succeed();
        });
    }
  });

  // Inject a script that activates the pre-rendered page.
  chrome.tabs.executeScript(
      tabId, {code: 'window.location.href = "./prerendering.html";'});
}

// Checks if navigation via chrome.tabs.update() doesn't activate pre-rendered
// pages. As chrome.tabs.update() causes a browser-side navigation, pre-rendered
// pages are not activated.
async function testDontActivateByUpdate() {
  await setup();

  // Start monitoring `empty.js` as this is requested only when a new
  // navigation is made for failed acitvations.
  chrome.webRequest.onBeforeRequest.addListener(function callback(details) {
    chrome.webRequest.onBeforeRequest.removeListener(callback);
    chrome.test.succeed();
  }, {urls: [getUrl('empty.js')]}, []);
  chrome.test.waitForRoundTrip('msg', () => {
    // Try activating the pre-rendered page, but it doesn't match and make a new
    // navigation as navigation parameters differ among the prerendering from
    // the initiator page and browser initiated navigations.
    chrome.tabs.update(tabId, {url: getUrl('prerendering.html')});
  });
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;
  chrome.test.assertNe(0, testServerPort);

  const tabs = await new Promise(
      resolve => chrome.tabs.query({active: true}, tabs => resolve(tabs)));
  chrome.test.assertEq(1, tabs.length);
  tabId = tabs[0].id;

  chrome.test.runTests([
    testGetTitleForAllFrames,
    testGetTitleByFrameId,
    testGetTitleByDocumentId,
    testActivateOnExecution,
    testExecuteAfterActivation,
    testDontActivateByUpdate,
  ]);
});
