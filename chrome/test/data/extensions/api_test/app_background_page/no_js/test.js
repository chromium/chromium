// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that setting allow_js_access to false is effective:
// - A background page is opened via window.open (which is verified by the
//   AppBackgroundPageApiTest.NoJsBackgroundPage code).
// - The return value of the window.open call is null (since the background
//   page is not scriptable)
// - Attempts to call window.open(...., "background") again will not result in
//   existing background page being closed and a new one being re-opened.

var pagePrefix =
    'http://a.com:PORT/extensions/api_test/app_background_page/no_js';
var launchUrl;
var launchTabId;
var backgroundPageLoaded = false;

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
// a background page whose name is "bg", which will succeed, but will not return
// a Window object. However, the background contents should load, which will
// then invoke onBackgroundPageLoaded.
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    launchUrl =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/launch.html';
    chrome.tabs.create(
        {url: launchUrl},
        function(tab) {
            launchTabId = tab.id;
        });
  });
}

function onBackgroundWindowNotNull() {
  chrome.test.notifyFail('Unexpected non-null window.open result');
}

function onBackgroundPageLoaded() {
  if (backgroundPageLoaded) {
    chrome.test.notifyFail('Background page loaded more than once.');
    return;
  }

  backgroundPageLoaded = true;

  // Close the existing page and re-open it, which will try to call
  // window.open(..., "background") again.
  chrome.tabs.remove(
      launchTabId,
      function() {
        chrome.tabs.create(
            {url: launchUrl },
            function(tab) {
              // We wait for a bit before declaring the test as passed, since
              // it might take a while for the additional background contents
              // to be recreated.
              setTimeout(chrome.test.notifyPass, 2000);
            });
      });
}
