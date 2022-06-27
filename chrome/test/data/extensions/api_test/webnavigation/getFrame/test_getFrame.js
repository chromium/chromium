// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inServiceWorker = 'ServiceWorkerGlobalScope' in self;
const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let ready;
let onScriptLoad = chrome.test.loadScript(scriptUrl);

const kNotSpecifiedErrorMessage =
  'Either documentId or both tabId and frameId must be specified.';

if (inServiceWorker) {
  ready = onScriptLoad;
} else {
  let onWindowLoad = new Promise((resolve) => {
    window.onload = resolve;
  });
  ready = Promise.all([onWindowLoad, onScriptLoad]);
}

ready.then(async function() {
  var URL = chrome.extension.getURL("a.html");
  var URL_FRAMES = chrome.extension.getURL("b.html");
  var processId = -1;
  var documentId;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    function testGetFrame() {
      var done = chrome.test.listenForever(chrome.webNavigation.onCommitted,
        function (details) {
          if (details.tabId != tab.id || details.url != URL)
            return;
          processId = details.processId;
          documentId = details.documentId;
          chrome.webNavigation.getFrame(
              {frameId: 0, tabId: tab.id, processId: processId},
              function(details) {
            chrome.test.assertEq(
                {errorOccurred: false,
                 url: URL,
                 parentFrameId: -1,
                 documentId: documentId,
                 documentLifecycle: "active",
                 frameType: "outermost_frame",
               },
                details);
            done();
          });
      });
      chrome.tabs.update(tab.id, {url: URL});
    },

    function testGetInvalidFrame() {
      chrome.webNavigation.getFrame(
          {tabId: tab.id, frameId: 1, processId: processId},
        function (details) {
          chrome.test.assertEq(null, details);
          chrome.test.succeed();
      });
    },

    function testGetFrameNoValues() {
      chrome.webNavigation.getFrame({},
        function (details) {
          chrome.test.assertEq(null, details);
          chrome.test.assertLastError(kNotSpecifiedErrorMessage);
          chrome.test.succeed();
      });
    },

    function testGetFrameNoFrameId() {
      chrome.webNavigation.getFrame({tabId: tab.id, processId: processId},
        function (details) {
          chrome.test.assertEq(null, details);
          chrome.test.assertLastError(kNotSpecifiedErrorMessage);
          chrome.test.succeed();
      });
    },

    function testGetFrameDocumentId() {
      chrome.webNavigation.getFrame({tabId: tab.id, documentId: documentId},
        function (details) {
          chrome.test.assertEq({
              errorOccurred: false,
              url: URL,
              parentFrameId: -1,
              documentId: documentId,
              documentLifecycle: "active",
              frameType: "outermost_frame",
            }, details);
          chrome.test.succeed();
      });
    },

    function testGetFrameDocumentIdAndFrameId() {
      chrome.webNavigation.getFrame({tabId: tab.id, frameId: 0,
                                     processId: processId,
                                     documentId: documentId},
        function (details) {
          chrome.test.assertEq({
              errorOccurred: false,
              url: URL,
              parentFrameId: -1,
              documentId: documentId,
              documentLifecycle: "active",
              frameType: "outermost_frame",
            }, details);
          chrome.test.succeed();
      });
    },

    function testGetFrameDocumentIdAndFrameIdDoNotMatch() {
      chrome.webNavigation.getFrame({tabId: tab.id, frameId: 1,
                                     processId: processId,
                                     documentId: documentId},
        function (details) {
          chrome.test.assertEq(null, details);
          chrome.test.succeed();
      });
    },

    function testGetFrameInvalidDocumentId() {
      chrome.webNavigation.getFrame({tabId: tab.id, frameId: 0,
                                     processId: processId,
                                     documentId: "42AB"},
        function (details) {
          chrome.test.assertLastError("Invalid documentId.");
          chrome.test.assertEq(null, details);
          chrome.test.succeed();
      });
    },

    function testGetAllFrames() {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, function (details) {
          chrome.test.assertEq(
              [{errorOccurred: false,
                frameId: 0,
                parentFrameId: -1,
                processId: processId,
                url: URL,
                documentId: documentId,
                documentLifecycle: "active",
                frameType: "outermost_frame"}],
               details);
          chrome.test.succeed();
      });
    },

    // Load an URL with a frame which is detached during load.
    // getAllFrames should only return the remaining (main) frame.
    async function testFrameDetach() {
      // TODO(crbug.com/1194800): Extremely flaky for Service Worker. Note that
      // this test is also (very infrequently) flaky for non-Service Worker.
      if (inServiceWorker)
        chrome.test.succeed();

      let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
      var done = chrome.test.listenForever(
        chrome.webNavigation.onCommitted,
        function (details) {
          if (details.tabId != tab.id || details.url != URL_FRAMES)
            return;
          processId = details.processId;
          documentId = details.documentId;
          chrome.webNavigation.getAllFrames(
              {tabId: tab.id},
            function (details) {
              chrome.test.assertEq(
                  [{errorOccurred: false,
                    frameId: 0,
                    parentFrameId: -1,
                    processId: processId,
                    url: URL_FRAMES,
                    documentId: documentId,
                    documentLifecycle: "active",
                    frameType: "outermost_frame"}],
                   details);
              chrome.test.succeed();
          });
      });
      chrome.tabs.update(tab.id, {url: URL_FRAMES});
    },
  ]);
});
