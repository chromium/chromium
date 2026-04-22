// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let firstWindowId;
let testTabId;

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  function resolveOnMessage(resolve) {
    chrome.runtime.onMessage.addListener(function local(message) {
      chrome.runtime.onMessage.removeListener(local);
      assertEq('relative.js', message);
      resolve();
    });
  }

  chrome.test.runTests([
    function setupRelativeUrlTests() {
      createWindow(['about:blank'], {}, pass(function(winId, tabIds) {
                     firstWindowId = winId;
                   }));
    },

    function relativeUrlTestsTabsCreate() {
      const onMessagePromise = new Promise(resolveOnMessage);

      const createPromise = new Promise((resolve) => {
        chrome.tabs.create(
            {windowId: firstWindowId, url: 'relative.html'}, function(tab) {
              testTabId = tab.id;
              resolve();
            });
      });

      Promise.all([onMessagePromise, createPromise]).then(chrome.test.succeed);
    },

    function relativeUrlTestsTabsUpdate() {
      const onMessagePromise = new Promise(resolveOnMessage);

      const updatePromise = new Promise((resolve) => {
        chrome.tabs.update(testTabId, {url: pageUrl('a')}, function(tab) {
          chrome.test.assertEq(pageUrl('a'), tab.pendingUrl);
          chrome.tabs.update(tab.id, {url: 'relative.html'}, function(tab) {
            resolve();
          });
        });
      });

      Promise.all([onMessagePromise, updatePromise]).then(chrome.test.succeed);
    },

    function relativeUrlTestsWindowCreate() {
      const onMessagePromise = new Promise(resolveOnMessage);

      const createPromise = new Promise((resolve) => {
        chrome.windows.create({url: 'relative.html'}, (window) => {
          resolve();
        });
      });

      Promise.all([onMessagePromise, createPromise]).then(chrome.test.succeed);
    },

  ]);
});
