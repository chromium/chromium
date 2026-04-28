// Copyright 2021 The Chromium Authors
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
  const urlMain = getURL('main.xml');
  chrome.test.runTests([
    // Navigate to an XML document with an XSLT stylesheet.
    // For the original XML document the extension should
    // receive onBeforeNavigate, onCommitted, and onDOMContentLoaded.
    // Then the XSLT is processed and the initial document is
    // replaced with a document that is the result of the XSLT
    // transformation. For this document the extension receives
    // a second onDOMContentLoaded followed by onCompleted.
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
          ],
          [
            [
              'main-onBeforeNavigate',
              'main-onCommitted',
              'main-onDOMContentLoaded',
            ],
            ['main-onDOMContentLoaded', 'main-onCompleted'],
          ]);

      chrome.tabs.update(tab.id, {url: `${urlMain}?${port}`});
    },
  ]);
});
