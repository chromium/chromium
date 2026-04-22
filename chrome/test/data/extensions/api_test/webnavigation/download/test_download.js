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

  const urlStart = `http://127.0.0.1:${port}/` +
      'extensions/api_test/webnavigation/download/a.html';
  const urlLoadRedirect = `http://127.0.0.1:${port}/server-redirect`;
  const urlNotFound = `http://127.0.0.1:${port}/not-found`;

  chrome.test.runTests([
    // Navigates to a page that redirects (on the server side) to a.html.
    function download() {
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
                url: urlStart
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
                transitionType: 'link',
                url: urlStart
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
                url: urlStart
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
                url: urlStart
              }
            }
          ],
          [navigationOrder('a-')]);
      chrome.tabs.update(tab.id, {url: `${urlStart}?${port}`});
    },
  ]);
});
