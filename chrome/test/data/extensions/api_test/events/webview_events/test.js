// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  var webview = document.createElement('webview');
  webview.src = 'data:text/html,<html><body>hello world</body></html>';
  webview.addEventListener('close', function() {});
  webview.contextMenus.onClicked.addListener(function() {});
  webview.contextMenus.onShow.addListener(function() {});
  webview.contextMenus.create({title: 'a', onclick: function() {}});
  webview.addEventListener('loadabort', () => { chrome.test.notifyFail(); });
  webview.addEventListener('loadstop', () => { chrome.test.notifyPass(); });
  webview.request.onMessage.addListener(function() {});
  document.body.appendChild(webview);
});
