// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  let getURL = chrome.runtime.getURL;
  let tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  let URL_MAIN = getURL('main.html');
  let URL_INTERMEDIATE_IFRAME = getURL('iframe.html');
  let URL_FENCED_FRAME = 'https://a.test:' + port +
      '/extensions/api_test/webnavigation/fencedFrames/frame.html';

  chrome.test.runTests([
    // Navigates from an extension page to a HTTP page to contain
    // an iframe which contains a fenced frame.
    // Tests that the frameId/parentFrameId are populated correctly.
    function fencedFrameNavigation() {
      expect([
        { label: 'main-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'main-onCommitted',
          event: 'onCommitted',
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'link',
                     url: URL_MAIN }},
        { label: 'main-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'main-onCompleted',
          event: 'onCompleted',
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'intermediate-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onCommitted',
          event: 'onCommitted',
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'auto_subframe',
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onCompleted',
          event: 'onCompleted',
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'a.test-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { documentLifecycle: "active",
                     frameId: 2,
                     frameType: "fenced_frame",
                     parentDocumentId: 2,
                     parentFrameId: 1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }},
        { label: 'a.test-onCommitted',
          event: 'onCommitted',
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "fenced_frame",
                     parentDocumentId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'auto_subframe',
                     url: URL_FENCED_FRAME }},
        { label: 'a.test-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "fenced_frame",
                     parentDocumentId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }},
        { label: 'a.test-onCompleted',
          event: 'onCompleted',
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "fenced_frame",
                     parentDocumentId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }}],
        [
          navigationOrder('main-'),
          navigationOrder('intermediate-'),
          navigationOrder('a.test-'),
        ]);

      chrome.tabs.update(tab.id, {url: URL_MAIN});
    },

    function testGetAllFrames() {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, function (details) {
          var documentIds = [];
          var nextDocumentId = 1;
          details.forEach(element => {
            // Since processIds are randomly assigned we remove them for the
            // assertEq.
            delete element.processId;
            if ('parentDocumentId' in element) {
              if (documentIds[element.parentDocumentId] === undefined) {
                documentIds[element.parentDocumentId] = nextDocumentId++;
              }
              element.parentDocumentId = documentIds[element.parentDocumentId];
            }
            if (documentIds[element.documentId] === undefined) {
              documentIds[element.documentId] = nextDocumentId++;
            }
            element.documentId = documentIds[element.documentId];
          });
          chrome.test.assertEq(
              [{errorOccurred: false,
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                url: URL_MAIN},
              {errorOccurred: false,
                documentId: 2,
                documentLifecycle: "active",
                frameId: 4,
                frameType: "sub_frame",
                parentDocumentId: 1,
                parentFrameId: 0,
                url: URL_INTERMEDIATE_IFRAME},
              {errorOccurred: false,
                documentId: 3,
                documentLifecycle: "active",
                frameId: 6,
                frameType: "fenced_frame",
                parentDocumentId: 2,
                parentFrameId: 4,
                url: URL_FENCED_FRAME}],
               details);
          chrome.test.succeed();
      });
    },
  ]);
});
