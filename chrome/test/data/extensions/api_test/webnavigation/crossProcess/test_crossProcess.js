// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  let getURL = chrome.runtime.getURL;

  var URL_REGULAR =
      "http://127.0.0.1:" + port + "/extensions/api_test/webnavigation/" +
      "crossProcess/empty.html";
  var URL_REDIRECT = "http://www.a.com:" + port + "/server-redirect";
  var URL_TEST = "http://127.0.0.1:" + port + "/test";

  chrome.test.runTests([
    // Navigates from an extension page to a HTTP page which causes a
    // process switch.
    function crossProcess() {
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
                url: getURL('a.html')
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
                url: getURL('a.html')
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
                url: getURL('a.html')
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
                url: getURL('a.html')
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
                url: URL_REGULAR
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
                url: URL_REGULAR
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
                url: URL_REGULAR
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
                url: URL_REGULAR
              }
            }
          ],
          [navigationOrder('a-'), navigationOrder('b-')]);

      chrome.tabs.update(tab.id, {url: getURL('a.html?' + port)});
    },

    // Redirects through an app extent, should cause two process switches.
    function crossProcessRedirect() {
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
                url: getURL('c.html')
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
                url: getURL('c.html')
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
                url: getURL('c.html')
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
                url: getURL('c.html')
              }
            },
            {
              label: 'c-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: "active",
                frameId: 0,
                frameType: "outermost_frame",
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: URL_REDIRECT
              }
            },
            {
              label: 'c-onCommitted',
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
                transitionQualifiers: ['server_redirect'],
                transitionType: 'link',
                url: URL_REGULAR
              }
            },
            {
              label: 'c-onDOMContentLoaded',
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
                url: URL_REGULAR
              }
            },
            {
              label: 'c-onCompleted',
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
                url: URL_REGULAR
              }
            }
          ],
          [
            navigationOrder('a-'),
            navigationOrder('c-'),
          ]);

      chrome.tabs.update(tab.id, {url: getURL('c.html?' + port)});
    },

    // Navigates to a different site, but then commits
    // same-site, non-user, renderer-initiated navigation
    // before the slow cross-site navigation commits.
    /*
     * This test case is disabled, because it is flaky and fails fairly
     * consistently on MSan bots. See https://crbug.com/467800
     *
    function crossProcessWithSameSiteCommit() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('d.html') }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "1" }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: URL_TEST + "1" }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "1" }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "1" }},
        { label: "c-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }},
        { label: "c-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('empty.html') }},
        { label: "c-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }},
        { label: "c-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }}],
        [ navigationOrder("a-"),
          navigationOrder("c-"),
          navigationOrder("b-"),
          [ "a-onCompleted", "b-onBeforeNavigate", "c-onBeforeNavigate",
            "c-onCommitted", "b-onCommitted"] ]);

      // Note: d.html expects the redirect path to follow the port
      // number.
      chrome.tabs.update(
          tab.id,
          { url: getURL('d.html?' + config.testServer.port + "/test1") });
    },
    */

    // Navigates cross-site, but then starts a same-site,
    // renderer-initiated navigation with user gesture.
    // The expectation is that the cross-process navigation
    // will be cancelled and the API should dispatch an onErrorOccurred
    // event.
    function crossProcessAbortUserGesture() {
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
                     url: getURL('d.html') }},
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
                     url: getURL('d.html') }},
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
                     url: getURL('d.html') }},
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
                     url: getURL('d.html') }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "2" }},
        { label: "b-onErrorOccurred",
          event: "onErrorOccurred",
          details: { error: "net::ERR_ABORTED",
                     documentId: 2,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_TEST + "2" }},
        { label: "c-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }},
        { label: "c-onCommitted",
          event: "onCommitted",
          details: { documentId: 3,
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
        { label: "c-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }},
        { label: "c-onCompleted",
          event: "onCompleted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('empty.html') }}],
        [ navigationOrder("a-"),
          navigationOrder("c-"),
          [ "a-onCompleted", "b-onBeforeNavigate", "b-onErrorOccurred",
            "c-onCommitted"] ]);

      // Note: d.html expects the redirect path to follow the port
      // number.
      chrome.tabs.update(tab.id, {url: getURL('d.html?' + port + "/test2")});
    },
  ]);
});
