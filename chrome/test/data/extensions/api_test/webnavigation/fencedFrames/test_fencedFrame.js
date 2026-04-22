// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/webnavigation/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  const getURL = chrome.runtime.getURL;
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);

  const port = config.testServer.port;
  const urlMain = getURL('main.html');
  const urlIntermediateIframe = getURL('iframe.html');
  const urlFencedFrame = `https://a.test:${port}/` +
      'extensions/api_test/webnavigation/fencedFrames/frame.html';

  chrome.test.runTests([
    // Navigates from an extension page to a HTTP page to contain
    // an iframe which contains a fenced frame.
    // Tests that the frameId/parentFrameId are populated correctly.
    function fencedFrameNavigation() {
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
              label: 'intermediate-onBeforeNavigate',
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
                url: urlIntermediateIframe,
              },
            },
            {
              label: 'intermediate-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'auto_subframe',
                url: urlIntermediateIframe,
              },
            },
            {
              label: 'intermediate-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlIntermediateIframe,
              },
            },
            {
              label: 'intermediate-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 1,
                parentFrameId: 0,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlIntermediateIframe,
              },
            },
            {
              label: 'a.test-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 2,
                frameType: 'fenced_frame',
                parentDocumentId: 2,
                parentFrameId: 1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: urlFencedFrame,
              },
            },
            {
              label: 'a.test-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 2,
                frameType: 'fenced_frame',
                parentDocumentId: 2,
                parentFrameId: 1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'auto_subframe',
                url: urlFencedFrame,
              },
            },
            {
              label: 'a.test-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 2,
                frameType: 'fenced_frame',
                parentDocumentId: 2,
                parentFrameId: 1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: urlFencedFrame,
              },
            },
            {
              label: 'a.test-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 3,
                documentLifecycle: 'active',
                frameId: 2,
                frameType: 'fenced_frame',
                parentDocumentId: 2,
                parentFrameId: 1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: urlFencedFrame,
              },
            },
          ],
          [
            navigationOrder('main-'),
            navigationOrder('intermediate-'),
            navigationOrder('a.test-'),
          ]);

      chrome.tabs.update(tab.id, {url: urlMain});
    },

    function testGetAllFrames() {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, function(details) {
        const documentIds = new Map();
        const frameIds = new Map();
        const toRelativeId = (id, map) => {
          if (id < 0) {
            return id;
          }
          if (!map.has(id)) {
            map.set(id, map.size);
          }
          return map.get(id);
        };

        details.forEach(element => {
          // Since processIds are randomly assigned we remove them for the
          // assertEq.
          delete element.processId;
          element.documentId = toRelativeId(element.documentId, documentIds);
          element.frameId = toRelativeId(element.frameId, frameIds);
          if ('parentDocumentId' in element) {
            element.parentDocumentId =
                toRelativeId(element.parentDocumentId, documentIds);
          }
          if ('parentFrameId' in element) {
            element.parentFrameId =
                toRelativeId(element.parentFrameId, frameIds);
          }
        });
        chrome.test.assertEq(
            [
              {
                errorOccurred: false,
                documentId: 0,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                url: urlMain,
              },
              {
                errorOccurred: false,
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 1,
                frameType: 'sub_frame',
                parentDocumentId: 0,
                parentFrameId: 0,
                url: urlIntermediateIframe,
              },
              {
                errorOccurred: false,
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 2,
                frameType: 'fenced_frame',
                parentDocumentId: 1,
                parentFrameId: 1,
                url: urlFencedFrame,
              },
            ],
            details);
        chrome.test.succeed();
      });
    },
  ]);
});
