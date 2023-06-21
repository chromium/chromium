// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let didCapture = false;
let didStopCapture = false;
let onStoppedCapture;
let captureStopped = new Promise((resolve) => {
  onStoppedCapture = resolve;
});

// Listen for changes in tabCapture state.
chrome.tabCapture.onStatusChanged.addListener((info) => {
  if (info.status == 'active') {
    didCapture = true;
  }
  if (info.status == 'stopped' && didCapture) {
    didStopCapture = true;
    onStoppedCapture();
  }
});

// TabCapture requires granted active tab. Wait for an action click to
// run tests.
chrome.action.onClicked.addListener(() => {
  chrome.test.runTests([
    // tabCapture.capture() doesn't work in service workers, since
    // MediaStreams are unsupported. It should be unreachable
    // (undefined on the API object).
    async function captureIsUndefined() {
      chrome.test.assertEq(undefined, chrome.tabCapture.capture);
      chrome.test.succeed();
    },

    // Create an offscreen document and capture the current tab.
    async function createDocumentAndStartCapture() {
      // Create a new offscreen document.
      await chrome.offscreen.createDocument(
          {
              url: 'offscreen.html',
              reasons: ['USER_MEDIA'],
              justification: 'testing'
          });
      // Create a capture stream for the currently-active tab.
      const tabs = await chrome.tabs.query({});
      chrome.test.assertEq(1, tabs.length);
      const tab = tabs[0];
      const streamId =
          await chrome.tabCapture.getMediaStreamId({targetTabId: tab.id});
      chrome.test.assertTrue(!!streamId);
      // Tell the offscreen document to start capturing.
      let response =
          await chrome.runtime.sendMessage(
              {command: 'capture', streamId: streamId});
      chrome.test.assertEq('success', response);
      chrome.test.assertTrue(didCapture);
      chrome.test.assertFalse(didStopCapture);

      // Verify the tab is currently being captured (and test
      // `tabCapture.getCapturedTabs()`).
      const capturedTabs = await chrome.tabCapture.getCapturedTabs();
      chrome.test.assertEq(1, capturedTabs.length);
      chrome.test.assertEq('active', capturedTabs[0].status);
      chrome.test.assertEq(tab.id, capturedTabs[0].tabId);

      // Stop capturing and verify.
      response = await chrome.runtime.sendMessage({command: 'stop'});
      chrome.test.assertEq('success', response);
      // In theory, there could a race between when we receive the
      // response from the offscreen document that it stopped the
      // capture and when the event indicating capture state fires.
      // Wait for it to account for either ordering.
      await captureStopped;
      chrome.test.assertTrue(didStopCapture);
      chrome.test.succeed();
    }
  ]);
});
