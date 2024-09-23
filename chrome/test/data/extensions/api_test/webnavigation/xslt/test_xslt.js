// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  debug = true;
  let getURL = chrome.runtime.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  let URL_MAIN = getURL('main.xml');
  let PATH_FRAME =
    '/extensions/api_test/webnavigation/xslt/frame.xml';
  chrome.test.runTests([
    // Navigate to an XML document with an XSLT stylesheet.
    // For the original XML document the extension should
    // receive onBeforeNavigate, onCommitted, and onDOMContentLoaded.
    // Then the XSLT is processed and the initial document is
    // replaced with a document that is the result of the XSLT
    // transformation. For this document the extension receives
    // a second onDOMContentLoaded followed by onCompleted.
    function crossProcessIframe() {
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
                   url: URL_MAIN }}],
        [
          [ "main-onBeforeNavigate",
            "main-onCommitted",
           "main-onDOMContentLoaded"],
           ["main-onDOMContentLoaded",
           "main-onCompleted"],
        ]);

      chrome.tabs.update(tab.id, {url: URL_MAIN + '?' + port});
    },
  ]);
});
