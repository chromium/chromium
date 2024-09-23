// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var myTabId;

function pageUrl(letter) {
  return chrome.runtime.getURL(letter + ".html");
}

function withTabOnReload(fn) {
  let onMessagePromise = new Promise((resolve) => {
    chrome.runtime.onMessage.addListener(function local(message) {
      chrome.runtime.onMessage.removeListener(local);
      chrome.test.assertEq(pageUrl("a"), message);
      resolve();
    });
  });
  let functionPromise = new Promise((resolve) => {
    fn(resolve);
  });
  Promise.all([onMessagePromise, functionPromise]).then(chrome.test.succeed);
}

chrome.test.runTests([
  function createTab() {
    withTabOnReload(function(resolve) {
      chrome.tabs.create({url: pageUrl("a"), selected: true}, function(tab) {
        myTabId = tab.id;
        resolve();
      });
    });
  },

  function testReload1() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload();
      resolve();
    });
  },

  function testReload2() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(null);
      resolve();
    });
  },

  function testReload3() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(myTabId);
      resolve();
    });
  },

  function testReload4() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(myTabId, {});
      resolve();
    });
  },

  function testReload5() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(myTabId, {}, resolve);
    });
  },

  function testReload6() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(myTabId, { bypassCache: false }, resolve);
    });
  },

  function testReload7() {
    withTabOnReload(function(resolve) {
      chrome.tabs.reload(myTabId, { bypassCache: true }, resolve);
    });
  },
]);
