// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var webview;

window.createGuest = function() {
  webview = document.createElement('webview');
  webview.src = 'about:blank';
  document.body.appendChild(webview);
  chrome.test.sendMessage('GuestAddedToDom');
}

window.loadGuest = function(port) {
  window.console.log('embedder.loadGuest: ' + port);

  // This page is not loaded, we just need a https URL.
  var guestSrcHTTPS = 'https://localhost:' + port +
      '/extensions/platform_apps/web_view/' +
      'interstitial_teardown/https_page.html';
  window.console.log('guestSrcHTTPS: ' + guestSrcHTTPS);
  webview.setAttribute('src', guestSrcHTTPS);
  webview.style.position = 'fixed';
  webview.style.left = '0px';
  webview.style.top = '0px';

  chrome.test.sendMessage('GuestLoaded');
};

window.loadGuestUrl = function(url) {
  window.console.log('embedder.loadGuest: ' + url);
  webview.setAttribute('src', url);
  webview.style.position = 'fixed';
  webview.style.left = '0px';
  webview.style.top = '0px';
  chrome.test.sendMessage('GuestLoaded');
};

window.onload = function() {
  chrome.test.sendMessage('EmbedderLoaded');
};
