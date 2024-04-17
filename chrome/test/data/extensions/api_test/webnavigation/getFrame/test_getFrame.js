// Copyright 2013 The Chromium Authors
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
  var URL = chrome.runtime.getURL("a.html");
  var URL_FRAMES = chrome.runtime.getURL("b.html");
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
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
    async function testGetPrerenderingFrames() {
      // This test is not valid for MV3+ because it uses
      // chrome.tabs.executeScript. See crbug.com/332328868
      if (chrome.runtime.getManifest().manifest_version > 2) {
        chrome.test.succeed();
        return;
      }

      const urlPrefix =
      `http://a.test:${port}/extensions/api_test/webnavigation/getFrame/`;
      const initialUrl = urlPrefix + "a.html?initial";
      const prerenderTargetUrl = urlPrefix + "a.html";
      const initiatorUrl = urlPrefix + "prerender.html";

      let tab = await promise(chrome.tabs.create, {url: initialUrl});

      var done = chrome.test.listenForever(
        chrome.webNavigation.onCommitted,
        function (details) {
          // Ignore frames other than the pre-rendered frame.
          if (details.tabId != tab.id || details.url != prerenderTargetUrl)
            return;
          // prerendered main frame shouldn't have frameId = 0.
          if (details.frameId == 0)
            return;
          chrome.webNavigation.getAllFrames(
              {tabId: tab.id},
            function (frameDetails) {
              chrome.test.assertEq(
                  [{errorOccurred: false,
                    frameId: details.frameId,
                    parentFrameId: -1,
                    processId: details.processId,
                    url: prerenderTargetUrl,
                    documentId: details.documentId,
                    documentLifecycle: "prerender",
                    frameType: "outermost_frame"}],
                    frameDetails.filter(ob => ob.url === prerenderTargetUrl));
              done();
          });
      });

      // TODO(crbug.com/40206306): Modify the testcase to be triggering
      // concurrent multiple prerendering pages once it is supported.
      // Navigate to a page that initiates prerendering "a.html".
      chrome.tabs.update(tab.id, {"url": initiatorUrl});
    },
    async function testGetPrerenderingFramesAndSubframes() {
      const urlPrefix =
      `http://a.test:${port}/extensions/api_test/webnavigation/getFrame/`;
      const initialUrl = urlPrefix + "a.html?initial";
      const prerenderTargetUrl = urlPrefix + "c.html";
      const prerenderTargetSubframeUrl = urlPrefix + "a.html";
      const initiatorUrl = urlPrefix + "prerender_multipleframes.html";

      let tab = await promise(chrome.tabs.create, {"url": initialUrl});

      var done = chrome.test.listenForever(
        chrome.webNavigation.onCommitted,
        function (details) {
          // Ignore frames other than the pre-rendered subframe to ensure all
          // frames are loaded.
          if (details.tabId != tab.id ||
              details.url != prerenderTargetSubframeUrl)
            return;

          // A prerendered subframe is expected to have a parent.
          if (details.parentFrameId == -1)
            return;

          chrome.webNavigation.getAllFrames(
              {tabId: tab.id},
            function (frameDetails) {
              chrome.test.assertEq(
                  [{errorOccurred: false,
                    frameId: details.parentFrameId,
                    parentFrameId: -1,
                    processId: details.processId,
                    url: prerenderTargetUrl,
                    documentId: details.parentDocumentId,
                    documentLifecycle: "prerender",
                    frameType: "outermost_frame"},
                    {errorOccurred: false,
                    frameId: details.frameId,
                    parentFrameId: details.parentFrameId,
                    processId: details.processId,
                    url: prerenderTargetSubframeUrl,
                    documentId: details.documentId,
                    parentDocumentId: details.parentDocumentId,
                    documentLifecycle: "prerender",
                    frameType: "sub_frame"}],
                    frameDetails.filter(ob => ob.documentLifecycle ===
                      "prerender"));
              done();
          });
      });

      // Navigate to a page that initiates prerendering "c.html", which contains
      // a subframe "a.html".
      chrome.tabs.update(tab.id, {"url": initiatorUrl});
    },
    async function testGetPrerenderingFramesInNewTab() {
      // This test is not valid for MV3+ because it uses
      // chrome.tabs.executeScript. See crbug.com/332328868
      if (chrome.runtime.getManifest().manifest_version > 2) {
        chrome.test.succeed();
        return;
      }

      const urlPrefix =
          `http://a.test:${port}/extensions/api_test/webnavigation/getFrame/`;
      const initialUrl = urlPrefix + 'a.html?initial';
      const prerenderTargetUrl = urlPrefix + 'c.html';
      const initiatorUrl = urlPrefix + 'prerender_new_tab.html';

      let initiatorTab = await promise(chrome.tabs.create, {'url': initialUrl});

      var done = chrome.test.listenForever(
          chrome.webNavigation.onCommitted, function(details) {
            // Ignore frames other than the pre-rendered frame.
            if (details.url != prerenderTargetUrl)
              return;

            // Trigger prerendered frame activation upon the first navigation.
            // The prerender tab will not be in BrowserList when it is not
            // activated yet.
            if (details.documentLifecycle === 'prerender') {
              // Inject a script that activates the pre-rendered page.
              chrome.tabs.executeScript(
                  initiatorTab.id,
                  {code: 'document.getElementById(\'link\').click();'});
              return;
            }

            chrome.test.assertNe(initiatorTab.id, details.tabId);

            chrome.webNavigation.getAllFrames(
                {tabId: details.tabId}, function(frameDetails) {
                  chrome.test.assertEq(
                      [{
                        errorOccurred: false,
                        frameId: 0,
                        parentFrameId: -1,
                        processId: details.processId,
                        url: prerenderTargetUrl,
                        documentId: details.documentId,
                        documentLifecycle: 'active',
                        frameType: 'outermost_frame'
                      }],
                      frameDetails.filter(ob => ob.url === prerenderTargetUrl));
                  done();
                });
          });

      // Navigate to a page that initiates prerendering "c.html", which contains
      // a subframe "a.html".
      chrome.tabs.update(initiatorTab.id, {'url': initiatorUrl});
    },
    async function testGetActivatedPrerenderingFrames() {
      // This test is not valid for MV3+ because it uses
      // chrome.tabs.executeScript. See crbug.com/332328868
      if (chrome.runtime.getManifest().manifest_version > 2) {
        chrome.test.succeed();
        return;
      }

      const urlPrefix =
          `http://a.test:${port}/extensions/api_test/webnavigation/getFrame/`;
      const initialUrl = urlPrefix + "a.html?initial";
      const prerenderTargetUrl = urlPrefix + "a.html";
      const initiatorUrl = urlPrefix + "prerender.html";

      let tab = await promise(chrome.tabs.create, {"url": initialUrl});

      var done = chrome.test.listenForever(
        chrome.webNavigation.onCommitted,
        function (details) {
          // Ignore frames other than the pre-rendered frame.
          if (details.tabId != tab.id || details.url != prerenderTargetUrl)
            return;

          // Trigger prerendered frame activation upon the first navigation.
          if (details.documentLifecycle === 'prerender') {
            // Inject a script that activates the pre-rendered page.
            chrome.tabs.executeScript(tab.id,
              {code: 'window.location.href = "./a.html";'});
            return;
          }

          chrome.webNavigation.getAllFrames(
              {tabId: tab.id},
            function (frameDetails) {
              chrome.test.assertEq(
                  [{errorOccurred: false,
                    frameId: 0,
                    parentFrameId: -1,
                    processId: details.processId,
                    url: prerenderTargetUrl,
                    documentId: details.documentId,
                    documentLifecycle: "active",
                    frameType: "outermost_frame"}],
                    frameDetails.filter(ob => ob.url === prerenderTargetUrl));
              done();
          });
      });

      // Navigate to a page that initiates prerendering "a.html".
      chrome.tabs.update(tab.id, {"url": initiatorUrl});
    },
    // Load an URL with a frame which is detached during load.
    // getAllFrames should only return the remaining (main) frame.
    async function testFrameDetach() {
      // TODO(crbug.com/40758628): Extremely flaky for Service Worker. Note that
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
