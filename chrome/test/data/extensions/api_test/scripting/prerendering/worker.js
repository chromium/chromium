// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let tabId = 0;
let prerenderingFrameId = 0;
let prerenderingDocumentId = 0;

async function testGetTitleByFrameId() {
  const results = await chrome.scripting.executeScript({
    target: {tabId: tabId, frameIds: [prerenderingFrameId]},
    func: () => {
      return document.title;
    }
  });
  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq('prerendering', results[0].result);
  chrome.test.succeed();
}

async function testGetTitleByDocumentId() {
  const results = await chrome.scripting.executeScript({
    target: {tabId: tabId, documentIds: [prerenderingDocumentId]},
    func: () => {
      return document.title;
    }
  });
  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq('prerendering', results[0].result);
  chrome.test.succeed();
}

async function testActivationOnExecution() {
  chrome.scripting.executeScript(
      {
        target: {tabId: tabId, frameIds: [prerenderingFrameId]},
        func: async () => {
          return new Promise(resolve => {
            document.addEventListener('prerenderingchange', () => {
              resolve('ok');
            });
          });
        }
      },
      results => {
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('ok', results[0].result);
        chrome.test.succeed();
      });
  chrome.scripting.executeScript({
    target: {tabId: tabId},
    func: () => {
      window.location.href = './prerendering.html';
    }
  });
}

async function testEventRouter() {
  chrome.scripting.executeScript(
      {
        target: {tabId: tabId, frameIds: [prerenderingFrameId]},
        func: async () => {
          return new Promise(resolve => {
            chrome.storage.onChanged.addListener(function(
                changes, event_namespace) {
              resolve('ok');
            });

            chrome.storage.local.set({'test': 1}).then(() => {});
          });
        }
      },
      results => {
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('ok', results[0].result);
        chrome.test.succeed();
      });
}

chrome.test.getConfig(async config => {
  const tabs = await chrome.tabs.query({active: true});
  chrome.test.assertEq(1, tabs.length);
  tabId = tabs[0].id;

  // The pre-rendered frame includes a resource for empty.js. We use a
  // webRequest listener to intercept the call for the resource in order to
  // identify the frame / document of the prerendered page.
  const port = config.testServer.port;
  const baseUrl =
      `http://example.com:${port}/extensions/api_test/scripting/prerendering/`;
  const kInitiatorUrl = baseUrl + 'initiator.html';
  const kEmptyJsUrl = baseUrl + 'empty.js';
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

  chrome.test.runTests([
    // TODO(crbug.com/40857271): disabled due to flakiness.
    // testGetTitleByFrameId,
    // testGetTitleByDocumentId,
    testEventRouter,
    // testActivationOnExecution,
  ]);
});
