// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test runs through an expected use of a live background page:
// - A background page is opened via the manifest.
// - A live (web-extent) web page is loaded (a.html), which opens the background
//   page using a different name.
// - We wait for this background page to get loaded. If a visible tab appears
//   instead, the test fails, otherwise it succeeds as soon as the page is
//   loaded.

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

// Start the test by opening the first page in the app. This will create a
// background page whose name is "bg", and this should replace the background
// page created by the manifest (named "background").
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    var a_url =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/a.html';
    chrome.tabs.create({ 'url': a_url });
  });
}

// Background page opened.
function onBackgroundPageLoaded() {
  // If this gets called without failing the test (by opening a visible
  // background page) we're done.
  chrome.test.notifyPass();
}

// A second background page opened.
function onBackgroundPageResponded() {
  chrome.test.notifyFail("onBackgroundPageResponded called unexpectedly");
}

// The background counter check found an unexpected value (most likely caused
// by an unwanted navigation).
function onCounterError() {
  chrome.test.notifyFail("checkCounter found an unexpected value");
}
