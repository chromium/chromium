// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var webview = document.createElement('webview');
webview.addEventListener('loadstop', function() {
  window.parent.postMessage('WebViewTest.LAUNCHED', '*');
});
webview.addEventListener('loadabort', function() {
  window.parent.postMessage('WebViewTest.FAILURE', '*');
});
webview.src = 'data:text/html,<body>guest in sandboxed iframe</body>';
document.body.appendChild(webview);
