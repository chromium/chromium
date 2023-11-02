// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      '></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    chrome.test.fail('No <webview> element created');
    return null;
  }

  webview.addEventListener('permissionrequest', function(e) {
    if (e.permission == 'download') {
      var url = e.url;
      if (url.indexOf('expect-deny.zip') != -1) {
        e.request.deny();
      } else if (url.indexOf('expect-allow.zip') != -1) {
        e.request.allow();
      } else {
        // Ignore.
      }
    }
  });
  var loaded = false;
  webview.addEventListener('loadstop', function(e) {
    if (!loaded) {
      loaded = true;
      chrome.test.sendMessage('guest-loaded');
    }
  });
  return webview;
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
    embedder.guestURL = embedder.baseGuestURL +
        '/extensions/platform_apps/web_view/download/guest.html';
    embedder.setUpGuest_();
  });
};
