// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', async function() {
  let config = await promise(chrome.test.getConfig);

  let PATH_BASE = "/extensions/api_test/webnavigation/pendingDeletion";
  let PATH_MAIN_FRAME =  PATH_BASE + "/iframe_slow_to_unload.html";
  let PATH_SUB_FRAME = PATH_BASE + "/slow_to_unload.html";

  let URL_A1 = 'http://a.com:' + config.testServer.port + PATH_MAIN_FRAME;
  let URL_A2 = 'http://a.com:' + config.testServer.port + PATH_SUB_FRAME;
  let URL_A3 = 'http://a.com:' + config.testServer.port + "/title1.html";
  let URL_B3 = 'http://b.com:' + config.testServer.port + "/title1.html";

  chrome.test.runTests([
    // Navigate from A1(A2) to A3 (same-origin navigation).
    // On A3's commit, GetAllFrame should return {A3} only.
    async function getAllFramePendindDeletionSameOrigin() {

      // 1) Create a new tab. Navigate to A1(A2).
      let tab = await promise(chrome.tabs.create, {"url": URL_A1});

      // 2) Wait for the load to complete and then navigate to A3.
      chrome.webNavigation.onCompleted.addListener(function (details) {
        if (details.tabId != tab.id || details.url != URL_A2)
          return;
        chrome.tabs.update(tab.id, {url: URL_A3});
      });

      // 3) On A3's commit, check GetAllFrame returns only {A3}.
      chrome.webNavigation.onCommitted.addListener(function (details) {
        if (details.tabId != tab.id || details.url != URL_A3)
          return;

        chrome.webNavigation.getAllFrames({tabId: tab.id},
          function (details) {
            chrome.test.assertEq(1, details.length);
            chrome.test.assertEq(URL_A3, details[0].url);
            chrome.test.succeed();
        });
      });
    },

    // Navigate from A1(A2) to B3 (cross-origin navigation).
    // On B3's commit, GetAllFrame should return {B3} only.
    async function getAllFramePendingDeletionDifferentOrigin() {

      // 1) Create a new tab. Navigate to A1(A2).
      let tab = await promise(chrome.tabs.create, {"url": URL_A1});

      // 2) Wait for the load to complete and then navigate to B3.
      chrome.webNavigation.onCompleted.addListener(function (details) {
        if (details.tabId != tab.id || details.url != URL_A2)
          return;
        chrome.tabs.update(tab.id, {url: URL_B3});
      });

      // 3) On B3's commit, check GetAllFrame returns only {B3}.
      chrome.webNavigation.onCommitted.addListener(function (details) {
        if (details.tabId != tab.id || details.url != URL_B3)
          return;
        chrome.webNavigation.getAllFrames({tabId: tab.id}, function (details) {
          chrome.test.assertEq(1, details.length);
          chrome.test.assertEq(URL_B3, details[0].url);
          chrome.test.succeed();
        });
      });
    },

  ]); // chrome.test.runTest.
});
