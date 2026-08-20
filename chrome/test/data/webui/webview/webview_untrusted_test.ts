// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertThrows} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('WebviewUntrustedBasicTest', function() {
  function createWebview(): chrome.webviewTag.WebView {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    document.body.appendChild(webview);
    return webview;
  }

  test('BannedApisThrowInUntrusted', async () => {
    const webview = createWebview();
    await new Promise(resolve => {
      webview.src = 'data:text/html,<html><body>test</body></html>';
      webview.addEventListener('loadstop', resolve);
    });

    assertThrows(() => {
      webview.addContentScripts([
        {name: 'rule', matches: ['<all_urls>'], js: {code: 'true;'}},
      ]);
    });
    assertThrows(() => {
      webview.captureVisibleRegion(null, () => {});
    });
    assertThrows(() => {
      webview.executeScript({code: 'true;'});
    });
    assertThrows(() => {
      webview.insertCSS({code: 'body { background-color: red; }'});
    });
    assertThrows(() => {
      webview.loadDataWithBaseUrl('data:text/html,test', 'https://example.com');
    });
  });
});
