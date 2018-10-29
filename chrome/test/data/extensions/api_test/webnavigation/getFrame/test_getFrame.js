// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  var URL = chrome.extension.getURL("a.html");
  var URL_FRAMES = chrome.extension.getURL("b.html");
  var processId = -1;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    function testGetFrame() {
      var done = chrome.test.listenForever(chrome.webNavigation.onCommitted,
        function (details) {
          if (details.tabId != tab.id || details.url != URL)
            return;
          processId = details.processId;
          chrome.webNavigation.getFrame(
              {tabId: tab.id, frameId: 0, processId: processId},
              function(details) {
            chrome.test.assertEq(
                {errorOccurred: false, url: URL, parentFrameId: -1},
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

    function testGetAllFrames() {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, function (details) {
          chrome.test.assertEq(
              [{errorOccurred: false,
                frameId: 0,
                parentFrameId: -1,
                processId: processId,
                url: URL}],
               details);
          chrome.test.succeed();
      });
    },

    // Load an URL with a frame which is detached during load.
    // getAllFrames should only return the remaining (main) frame.
    async function testFrameDetach() {
      let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
      var done = chrome.test.listenForever(
        chrome.webNavigation.onCommitted,
        function (details) {
          if (details.tabId != tab.id || details.url != URL_FRAMES)
            return;
          processId = details.processId;
          chrome.webNavigation.getAllFrames(
              {tabId: tab.id},
            function (details) {
              chrome.test.assertEq(
                  [{errorOccurred: false,
                    frameId: 0,
                    parentFrameId: -1,
                    processId: processId,
                    url: URL_FRAMES}],
                   details);
              chrome.test.succeed();
          });
      });
      chrome.tabs.update(tab.id, {url: URL_FRAMES});
    },
  ]);
}
