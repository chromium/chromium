// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  var getURL = chrome.runtime.getURL;
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  var URL_TEST = "http://127.0.0.1:" + port + "/test";
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  chrome.test.runTests([
    // Navigates to a different site, but then modifies the history using
    // history.pushState().
    function crossProcessHistory() {
      expect(
          [
            {
              label: 'a-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: getURL('e.html')
              }
            },
            {
              label: 'a-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: getURL('e.html')
              }
            },
            {
              label: 'a-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: getURL('e.html')
              }
            },
            {
              label: 'a-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: getURL('e.html')
              }
            },
            {
              label: 'a-onHistoryStateUpdated',
              event: 'onHistoryStateUpdated',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: getURL('empty.html')
              }
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '2'
              }
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: URL_TEST + '2'
              }
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '2'
              }
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '2'
              }
            }
          ],
          [
            navigationOrder('a-'),
            [
              'a-onCompleted', 'b-onBeforeNavigate', 'a-onHistoryStateUpdated',
              'b-onCommitted'
            ]
          ]);

      chrome.tabs.update(tab.id, { url: getURL('e.html?' + port) });
    },

    // A page with an iframe that changes its history state using
    // history.pushState before the iframe is committed.
    function crossProcessHistoryIFrame() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('h.html') }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('h.html') }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('h.html') }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }},
        { label: "a-onHistoryStateUpdated",
          event: "onHistoryStateUpdated",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('empty.html') }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "5" }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: URL_TEST + "5" }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "5" }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "5" }}],
        [ navigationOrder("a-"), navigationOrder("b-"),
          [ "b-onBeforeNavigate", "a-onHistoryStateUpdated",
            "a-onCompleted"] ]);

      chrome.tabs.update(tab.id, {url: getURL('h.html?' + port)});
    },

    // Navigates to a different site, but then modifies the history using
    // history.replaceState().
    function crossProcessHistoryReplace() {
      expect(
          [
            {
              label: 'a-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: getURL('i.html')
              }
            },
            {
              label: 'a-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: getURL('i.html')
              }
            },
            {
              label: 'a-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: getURL('i.html')
              }
            },
            {
              label: 'a-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: getURL('i.html')
              }
            },
            {
              label: 'a-onHistoryStateUpdated',
              event: 'onHistoryStateUpdated',
              details: {
                documentId: 1,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: getURL('empty.html')
              }
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '6'
              }
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: URL_TEST + '6'
              }
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '6'
              }
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 2,
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: URL_TEST + '6'
              }
            }
          ],
          [
            navigationOrder('a-'),
            [
              'a-onCompleted', 'b-onBeforeNavigate', 'a-onHistoryStateUpdated',
              'b-onCommitted'
            ]
          ]);

      chrome.tabs.update(tab.id, {url: getURL('i.html?' + port)});
    },
  ]);
});
