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

// Checks that `tabId` can find prerendering tab.
async function testGetTabByTabId() {
  await setup();
  chrome.tabs.get(prerenderingTabId, function(tab) {
    // Make sure tab.windowId is chrome.windows.WINDOW_ID_NONE and it is
    // allowed.
    chrome.test.assertEq(chrome.windows.WINDOW_ID_NONE, tab.windowId);
    chrome.test.assertEq(-1, tab.index);
    chrome.test.succeed();
  });
}

// Tests that OnAttached is not called because chrome.tabs.move doesn't interact
// with invisible tabs.
async function testOnAttachedWithoutActivation() {
  await setup();
  let windowId = -1;
  let secondWindowId = -1;

  chrome.tabs.getSelected(
      null, pass(function(tab) {
        windowId = tab.windowId;
        waitForAllTabs(pass(function() {
          createWindow(
              [''], {}, pass(function(winId, tabIds) {
                secondWindowId = winId;
                chrome.test.assertNe(windowId, -1);
                chrome.test.assertNe(secondWindowId, -1);
                chrome.tabs.move(
                    prerenderingTabId, {'windowId': secondWindowId, 'index': 0},
                    function() {
                      chrome.test.assertEq(
                          chrome.runtime.lastError.message,
                          'No tab with id: ' + prerenderingTabId + '.');
                    });
              }));
        }));
      }));

  chrome.test.succeed();
}

// Tests that OnAttached is aware of the newly created prerendering into a new
// tab after activation.
async function testOnAttachedAfterActivation() {
  const activationCallback = details => {
    if (details.documentLifecycle === 'prerender') {
      chrome.tabs.executeScript(tabId, {
        code: `document.getElementById(\'link\').click();`,
        runAt: 'document_idle'
      });

      let windowId = -1;
      let secondWindowId = -1;
      chrome.tabs.getSelected(
          null, pass(function(tab) {
            windowId = tab.windowId;

            waitForAllTabs(pass(function() {
              createWindow([''], {}, pass(function(winId, tabIds) {
                             secondWindowId = winId;
                             chrome.test.listenOnce(
                                 chrome.tabs.onAttached,
                                 function(testTabId, info) {
                                   // Ensure notification is correct.
                                   assertEq(testTabId, prerenderingTabId);
                                   assertEq(winId, info.newWindowId);
                                   chrome.test.succeed();
                                 });

                             chrome.test.assertNe(windowId, -1);
                             chrome.test.assertNe(secondWindowId, -1);
                             chrome.tabs.move(
                                 prerenderingTabId,
                                 {'windowId': secondWindowId, 'index': 0},
                                 function() {});
                           }));
            }));
          }));
    }
  };

  chrome.webRequest.onCompleted.addListener(
      activationCallback, {urls: [getUrl('empty.js')]}, []);

  // This test is intended to check the behavior after activation, so it is
  // needed to set up activationCallback before calling setup function.
  await setup();
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;
  chrome.test.assertNe(0, testServerPort);

  const tabs = await new Promise(
      resolve => chrome.tabs.query({active: true}, tabs => resolve(tabs)));
  chrome.test.assertEq(1, tabs.length);
  tabId = tabs[0].id;

  await chrome.test.loadScript(
      '_test_resources/api_test/tabs/basics/tabs_util.js');

  // TODO(crbug.com/40234240): add more tests for tabs.on* event listeners.
  chrome.test.runTests([
    // TODO(crbug.com/40942071): Flaky on multiple platforms.
    // testGetTitleForAllFrames,
    // testGetAllInWindow,
    // testQuery,
    // testGetTitleByFrameId,
    // testGetTitleByDocumentId,
    testGetTabByTabId,
    testOnAttachedWithoutActivation,
    testOnAttachedAfterActivation,
  ]);
});
