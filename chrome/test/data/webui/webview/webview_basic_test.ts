// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('WebviewBasicTest', function() {
  test('DisplayNone', async () => {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    const loadStopped = new Promise<void>((resolve, reject) => {
      webview.addEventListener('loadstop', function() {
        document.body.style.display = 'none';
        // Give it some time (100ms) before making document.body visible again.
        window.setTimeout(function() {
          document.body.style.display = '';
          webview.addEventListener('loadstop', function() {
            resolve();
          });
          webview.reload();
        }, 100);
      });
      webview.addEventListener('loadabort', function() {
        reject();
      });
    });
    webview.src = 'about:blank';
    document.body.appendChild(webview);
    await loadStopped;
  });
});
