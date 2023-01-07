// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See chrome/browser/extensions/web_view_interactive_browsertest.cc
// (WebViewInteractiveTest, PointerLockFocus) for documentation on this test.
var guestURL;
var startTest = function(config) {
  window.addEventListener('message', receiveMessage, false);
  chrome.test.sendMessage('guest-loaded');
  var webview = document.getElementById('webview');
  webview.addEventListener('loadstop', function(e) {
    webview.contentWindow.postMessage('msg', '*');
  });
  webview.addEventListener('permissionrequest', function(e) {
    document.getElementById('embedder-textarea').focus();
    e.preventDefault();
    setTimeout(function() { e.request.allow(); }, 500);
  });
  webview.src = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/pointer_lock_focus/guest.html';
};
var receiveMessage = function(event) {
  chrome.test.sendMessage(event.data);
}

chrome.test.getConfig(function(config) {
  startTest(config);
});
