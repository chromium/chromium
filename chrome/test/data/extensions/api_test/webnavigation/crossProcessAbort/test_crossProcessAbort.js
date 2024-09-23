// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  let getURL = chrome.runtime.getURL;

  let INITIAL_URL = getURL("initial.html");
  let SAME_SITE_URL = getURL("empty.html");
  let CROSS_SITE_URL = "http://127.0.0.1:" + port + "/title1.html";

  chrome.test.runTests([
    // Navigates to a slow cross-site URL (extension to HTTP) and starts
    // a slow renderer-initiated, non-user, same-site navigation.
    // The cross-site navigation commits while the same-site navigation
    // is in process and this test expects an error event for the
    // same-site navigation.
    function crossProcessAbort() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: INITIAL_URL }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: INITIAL_URL }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: INITIAL_URL }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: INITIAL_URL }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: CROSS_SITE_URL }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: CROSS_SITE_URL }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: CROSS_SITE_URL }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: CROSS_SITE_URL }},
        { label: "c-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: SAME_SITE_URL }},
        { label: "c-onErrorOccurred",
          event: "onErrorOccurred",
          details: { error: "net::ERR_ABORTED",
                     frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: SAME_SITE_URL }}
       ],
        [ navigationOrder("a-"),
          navigationOrder("b-"),
          [ "a-onCompleted",
            "b-onBeforeNavigate",
            "c-onBeforeNavigate",
            "c-onErrorOccurred",
            "b-onCommitted" ]]);

      chrome.tabs.update(
        tab.id,
        { url: getURL("initial.html?" + port + "/title1.html") }
      );
    },
  ]);
};
