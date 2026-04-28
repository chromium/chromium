// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/webnavigation/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  debug = true;
  const getURL = chrome.runtime.getURL;
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;
  const urlMain = getURL('main.html');
  const PATH_FRAME =
      '/extensions/api_test/webnavigation/crossProcessIframe/frame.html';
  const urlFrame1 = `http://a.com:${port}${PATH_FRAME}`;
  const urlFrame2 = `http://b.com:${port}${PATH_FRAME}`;
  const urlFrame3 = `http://c.com:${port}${PATH_FRAME}`;
  chrome.test.runTests([
    // Navigates from an extension page to a HTTP page which causes a
    // process switch. The extension page embeds a same-process iframe which
    // embeds another frame that navigates three times (cross-process):
    // c. Loaded by the parent frame.
    // d. Navigated by the parent frame.
    // e. Navigated by the child frame.
    // Tests whether the frameId stays constant across navigations.
    function crossProcessIframe() {
      expect(
          [
            {
              label: 'main-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: urlMain,
              },
            },
            {
              label: 'main-onCommitted',
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
                transitionType: 'link',
                url: urlMain,
              },
            },
            {
              label: 'main-onDOMContentLoaded',
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
                url: urlMain,
              },
            },
            {
              label: 'main-onCompleted',
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
                url: urlMain,
              },
            },
            {
              label: 'a.com-onBeforeNavigate',
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
                url: urlFrame1,
              },
            },
            {
              label: 'a.com-onCommitted',
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
                url: urlFrame1,
              },
            },
            {
              label: 'a.com-onDOMContentLoaded',
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
                url: urlFrame1,
              },
            },
            {
              label: 'a.com-onCompleted',
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
                url: urlFrame1,
              },
            },
            {
              label: 'b.com-onBeforeNavigate',
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
                url: urlFrame2,
              },
            },
            {
              label: 'b.com-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 2,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'manual_subframe',
                url: urlFrame2,
              },
            },
            {
              label: 'b.com-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 2,
                tabId: 0,
                timeStamp: 0,
                url: urlFrame2,
              },
            },
            {
              label: 'b.com-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 2,
                tabId: 0,
                timeStamp: 0,
                url: urlFrame2,
              },
            },
            {
              label: 'c.com-onBeforeNavigate',
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
                url: urlFrame3,
              },
            },
            {
              label: 'c.com-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 4,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 3,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'manual_subframe',
                url: urlFrame3,
              },
            },
            {
              label: 'c.com-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 4,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 3,
                tabId: 0,
                timeStamp: 0,
                url: urlFrame3,
              },
            },
            {
              label: 'c.com-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 4,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 3,
                tabId: 0,
                timeStamp: 0,
                url: urlFrame3,
              },
            },
          ],
          [
            navigationOrder('main-'),
            navigationOrder('a.com-'),
            navigationOrder('b.com-'),
            navigationOrder('c.com-'),
            ['a.com-onCommitted', 'b.com-onBeforeNavigate'],
            ['b.com-onCommitted', 'c.com-onBeforeNavigate'],
          ]);

      chrome.tabs.update(tab.id, {url: `${urlMain}?${port}`});
    },

  ]);
});
