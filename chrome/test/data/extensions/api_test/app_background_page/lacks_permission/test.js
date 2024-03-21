// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test uses the same code path as the has_permission case, but only tests
// that a visible window was created that contains the bg.html page.

var pagePrefix =
    'http://a.com:PORT/extensions/api_test/app_background_page/common';

// Dispatch "tunneled" functions from the live web pages to this testing page.
chrome.runtime.onMessage.addListener(function(request) {
  window[request.name](request.args);
});

function onBackgroundPageLoaded() {
  chrome.test.notifyFail("BackgroundContents loaded without permission");
}

function onBackgroundPagePermissionDenied() {
  chrome.test.notifyPass();
}

// Start the test by opening the first page in the app.
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    var a_url =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/a.html';
    chrome.tabs.create({ 'url': a_url }, function(tab) {});
  });
}
