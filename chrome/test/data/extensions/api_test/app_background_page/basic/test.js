// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test runs through an expected use of a live background page:
// - A live (web-extent) web page is loaded (a.html), which opens the background
//   page.
// - The first page is closed and a second live web page is loaded (b.html),
//   which attempts to get still-running running background page. This second
//   page also checks a counter which should have a value consistent with being
//   called once from each of the first and second pages.
// - The background page closes itself.

var pageA;
var pageB;
var backgroundPageResponded = false;

var pagePrefix =
    'http://a.com:PORT/extensions/api_test/app_background_page/common';

// Dispatch "tunneled" functions from the live web pages to this testing page.
chrome.runtime.onMessage.addListener(function(request) {
  window[request.name](request.args);
});

// At no point should a window be created that contains the background page
// (bg.html).
chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (tab.url.match("bg\.html$")) {
    chrome.test.notifyFail("popup opened instead of background page");
  }
});

// Start the test by opening the first page in the app.
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    var a_url =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/a.html';
    chrome.tabs.create({ 'url': a_url }, function(tab) {
      pageA = tab;
    });
  });
}

// Background page opened by pageA.
function onBackgroundPageLoaded() {
  chrome.tabs.remove(pageA.id, function() {
    chrome.test.getConfig(function(config) {
      var b_url =
          pagePrefix.replace(/PORT/, config.testServer.port) + '/b.html';
      chrome.tabs.create({ url: b_url }, function(tab) {
        pageB = tab;
      });
    });
  });
}

// Background page responded to pageB.
function onBackgroundPageResponded() {
  backgroundPageResponded = true;
}

// Background page is closing itself.
function onBackgroundPageClosing() {
  if (!backgroundPageResponded) {
    chrome.test.notifyFail("background never responded to pageB");
  } else {
    chrome.test.notifyPass();
  }
}

// The background counter check found an unexpected value (most likely caused
// by an unwanted navigation.
function onCounterError() {
  chrome.test.notifyFail("checkCounter found an unexpected value");
}
