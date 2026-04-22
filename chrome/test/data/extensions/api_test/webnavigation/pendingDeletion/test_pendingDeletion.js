// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/webnavigation/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  const config = await promise(chrome.test.getConfig);

  const PATH_BASE = '/extensions/api_test/webnavigation/pendingDeletion';
  const pathMainFrame = `${PATH_BASE}/iframe_slow_to_unload.html`;
  const pathSubFrame = `${PATH_BASE}/slow_to_unload.html`;

  const urlA1 = `http://a.com:${config.testServer.port}${pathMainFrame}`;
  const urlA2 = `http://a.com:${config.testServer.port}${pathSubFrame}`;
  const urlA3 = `http://a.com:${config.testServer.port}/title1.html`;
  const urlB3 = `http://b.com:${config.testServer.port}/title1.html`;

  chrome.test.runTests([
    // Navigate from A1(A2) to A3 (same-origin navigation).
    // On A3's commit, GetAllFrame should return {A3} only.
    async function getAllFramePendindDeletionSameOrigin() {
      // 1) Create a new tab. Navigate to A1(A2).
      const tab = await promise(chrome.tabs.create, {url: urlA1});

      // 2) Wait for the load to complete and then navigate to A3.
      chrome.webNavigation.onCompleted.addListener(function(details) {
        if (details.tabId != tab.id || details.url != urlA2) {
          return;
        }
        chrome.tabs.update(tab.id, {url: urlA3});
      });

      // 3) On A3's commit, check GetAllFrame returns only {A3}.
      chrome.webNavigation.onCommitted.addListener(function(details) {
        if (details.tabId != tab.id || details.url != urlA3) {
          return;
        }

        chrome.webNavigation.getAllFrames({tabId: tab.id}, function(details) {
          chrome.test.assertEq(1, details.length);
          chrome.test.assertEq(urlA3, details[0].url);
          chrome.test.succeed();
        });
      });
    },

    // Navigate from A1(A2) to B3 (cross-origin navigation).
    // On B3's commit, GetAllFrame should return {B3} only.
    async function getAllFramePendingDeletionDifferentOrigin() {
      // 1) Create a new tab. Navigate to A1(A2).
      const tab = await promise(chrome.tabs.create, {url: urlA1});

      // 2) Wait for the load to complete and then navigate to B3.
      chrome.webNavigation.onCompleted.addListener(function(details) {
        if (details.tabId != tab.id || details.url != urlA2) {
          return;
        }
        chrome.tabs.update(tab.id, {url: urlB3});
      });

      // 3) On B3's commit, check GetAllFrame returns only {B3}.
      chrome.webNavigation.onCommitted.addListener(function(details) {
        if (details.tabId != tab.id || details.url != urlB3) {
          return;
        }
        chrome.webNavigation.getAllFrames({tabId: tab.id}, function(details) {
          chrome.test.assertEq(1, details.length);
          chrome.test.assertEq(urlB3, details[0].url);
          chrome.test.succeed();
        });
      });
    },

  ]);  // chrome.test.runTest.
});
