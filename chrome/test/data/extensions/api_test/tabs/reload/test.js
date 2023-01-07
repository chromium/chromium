// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var callbackPass = chrome.test.callbackPass;

var myTabId;

// window.onload in a.html calls this function.
var onTabLoad;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
}

function withTabOnReload(fn) {
  var done = callbackPass();
  onTabLoad = function(url) {
    assertEq(pageUrl("a"), url);
    done();
  };
  fn();
}

var allTests = [
  function testReload1() {
    withTabOnReload(function () {
      chrome.tabs.reload();
    });
  },

  function testReload2() {
    withTabOnReload(function () {
      chrome.tabs.reload(null);
    });
  },

  function testReload2() {
    withTabOnReload(function () {
      chrome.tabs.reload(myTabId);
    });
  },

  function testReload4() {
    withTabOnReload(function () {
      chrome.tabs.reload(myTabId, {});
    });
  },

  function testReload5() {
    withTabOnReload(function () {
      chrome.tabs.reload(myTabId, {}, callbackPass());
    });
  },

  function testReload6() {
    withTabOnReload(function () {
      chrome.tabs.reload(myTabId, { bypassCache: false }, callbackPass());
    });
  },

  function testReload7() {
    withTabOnReload(function () {
      chrome.tabs.reload(myTabId, { bypassCache: true }, callbackPass());
    });
  },
];

onTabLoad = function(url) {
  chrome.test.runTests(allTests);
};

chrome.tabs.create({url: pageUrl("a")}, function(tab) {
  myTabId = tab.id;
  chrome.tabs.update(myTabId, { selected: true });
});
