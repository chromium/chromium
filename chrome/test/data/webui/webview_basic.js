// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDisplayNone() {
  const webview = document.createElement('webview');
  webview.onloadstop = function() {
    document.body.style.display = 'none';
    // Give it some time (100ms) before making document.body visible again.
    window.setTimeout(function() {
      document.body.style.display = '';
      webview.onloadstop = function() {
        chrome.send('testResult', [true]);
      };
      webview.reload();
    }, 100);
  };
  webview.onloadabort = function() {
    chrome.send('testResult', [false]);
  };
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}
