// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test closes the existing background page opened by a.html.

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
    var closeUrl =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/close.html';
    chrome.tabs.create({ 'url': closeUrl }, function(tab) {});
  });
};

// Background page is closed now.
function onBackgroundPageClosed() {
  chrome.test.notifyPass();
};
