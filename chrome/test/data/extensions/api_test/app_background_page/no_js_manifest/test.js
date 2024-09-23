// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that setting allow_js_access to false is effective:
// - A background page is opened via the manifest (which is verified by the
//   AppBackgroundPageApiTest.NoJsManifestBackgroundPage code).
// - A live (web-extent) web page is loaded (a.html), which tries to opens a
//   background page.  This fails because allow_js_access is false.

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

// Start the test by opening the first page in the app. This will try to create
// a background page whose name is "bg", but it should not replace the
// background page created by the manifest (named "background").
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    var aUrl =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/a.html';
    chrome.tabs.create({ 'url': aUrl });
  });
}

// Background page opened.
function onBackgroundPageLoaded() {
  // The window.open call in a.html should not succeed.
  chrome.test.notifyFail("Background page unexpectedly loaded.");
}

function onBackgroundPagePermissionDenied() {
  // a.html will call this if it receives null from window.open, as we expect.
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
