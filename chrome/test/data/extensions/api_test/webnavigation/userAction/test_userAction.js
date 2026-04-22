// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/webnavigation/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  const getURL = chrome.runtime.getURL;
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;

  const urlMain = getURL('a.html');
  const subframeUrl = `http://127.0.0.1:${port}/` +
      'extensions/api_test/webnavigation/userAction/subframe.html';

  chrome.test.runTests([
    // Opens a tab and waits for the user to click on a link in it.
    function userAction() {
      expect(
          [
            {
              label: 'a-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: urlMain
              }
            },
            {
              label: 'a-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'typed',
                url: urlMain
              }
            },
            {
              label: 'a-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlMain
              }
            },
            {
              label: 'a-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlMain
              }
            },

            {
              label: 'subframe-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: subframeUrl
              }
            },
            {
              label: 'subframe-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'auto_subframe',
                url: subframeUrl
              }
            },
            {
              label: 'subframe-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: subframeUrl
              }
            },
            {
              label: 'subframe-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: subframeUrl
              }
            },

            {
              label: 'b-onCreatedNavigationTarget',
              event: 'onCreatedNavigationTarget',
              details: {
                sourceFrameId: 1,
                sourceProcessId: 1,
                sourceTabId: 0,
                tabId: 1,
                timeStamp: 0,
                url: getURL('b.html')
              }
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 1,
                timeStamp: 0,
                url: getURL('b.html')
              }
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: getURL('b.html')
              }
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                url: getURL('b.html')
              }
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                url: getURL('b.html')
              }
            }
          ],
          [
            navigationOrder('a-'), navigationOrder('subframe-'),
            navigationOrder('b-'),
            [
              'a-onCompleted', 'b-onCreatedNavigationTarget',
              'b-onBeforeNavigate'
            ]
          ]);

      // Notify the api test that we're waiting for the user.
      chrome.test.notifyPass();
    },
  ]);
});
