// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  let getURL = chrome.extension.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;

  let URL_START = "http://127.0.0.1:" + port +
    "/extensions/api_test/webnavigation/download/a.html";
  let URL_LOAD_REDIRECT = "http://127.0.0.1:" + port + "/server-redirect";
  let URL_NOT_FOUND = "http://127.0.0.1:" + port + "/not-found";

  chrome.test.runTests([
    // Navigates to a page that redirects (on the server side) to a.html.
    function download() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_START }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: URL_START }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_START }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_START }}],
      [ navigationOrder("a-") ]);
      chrome.tabs.update(tab.id, { url: URL_START + "?" + port });
    },
  ]);
};
