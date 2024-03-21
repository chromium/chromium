// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test runs through an expected use of a live background page:
// - A live (web-extent) web page is loaded (a.html), which opens the background
//   page.
// - The first page is closed and a second live web page is loaded (c.html),
//   which uses a different name. This second page also checks a counter which
//   should have a value consistent with being called once.
// - We then reopen the first page. This first page also checks a counter
//   which should have a value consistent with the page being newly opened.
// - The background page closes itself.

var pageA;
var pageC;
var step = 0;

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

// Background page opened.
function onBackgroundPageLoaded() {
  // There are 3 steps to this test:
  // #1: page A just opened and opened its background page, so we close pageA
  //     and open pageC.
  // #2: page C just opened and opened its background page, so we close pageC
  //     and reopen pageA.
  // #3: page A opened again and opened its background page, so we're done.
  if (step == 0) {
    // Close A, open C.
    chrome.tabs.remove(pageA.id, function() {
      chrome.test.getConfig(function(config) {
        var c_url =
            pagePrefix.replace(/PORT/, config.testServer.port) + '/c.html';
        chrome.tabs.create({ url: c_url }, function(tab) {
          pageC = tab;
        });
      });
    });
  } else if (step == 1) {
    // Close C, re-open A
    chrome.tabs.remove(pageC.id, function() {
      chrome.test.getConfig(function(config) {
        var a_url =
            pagePrefix.replace(/PORT/, config.testServer.port) + '/a.html';
        chrome.tabs.create({ url: a_url }, function(tab) {
          pageA = tab;
        });
      });
    });
  } else if (step == 2) {
    chrome.test.notifyPass();
  } else {
    chrome.test.notifyFail("onBackgroundPageLoaded() called too many times");
  }
  step++;
}

// Background page responded to pageC.
function onBackgroundPageResponded() {
  chrome.test.notifyFail("onBackgroundPageResponded called unexpectedly");
}

// The background counter check found an unexpected value (most likely caused
// by an unwanted navigation.
function onCounterError() {
  chrome.test.notifyFail("checkCounter found an unexpected value");
}
