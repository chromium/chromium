// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('WebviewBasicTest', function() {
  function createWebview(): chrome.webviewTag.WebView {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    document.body.appendChild(webview);
    return webview;
  }

  async function webviewLoadStopped(webview: chrome.webviewTag.WebView) {
    return new Promise<void>(resolve => {
      webview.addEventListener('loadstop', () => {
        resolve();
      });
    });
  }

  function getWebviewUrl(): string {
    return (window as unknown as Window & {webviewUrl: string}).webviewUrl;
  }

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

  async function testMediaRequest(allowRequest: boolean) {
    const webview = createWebview();
    webview.addEventListener('permissionrequest', (e: any) => {
      if (e.permission === 'media') {
        if (allowRequest) {
          e.request.allow();
        } else {
          e.request.deny();
        }
      }
    });

    webview.src = getWebviewUrl();
    await webviewLoadStopped(webview);

    const result = new Promise<boolean>(resolve => {
      window.addEventListener('message', (e: any) => {
        if (e.data.granted !== undefined) {
          resolve(e.data.granted);
        }
      });
    });

    // Send a message so that the webview can send a reply.
    webview.contentWindow!.postMessage({}, '*');
    return result;
  }

  // Verifies that a webview within a webui can forward media requests
  // successfully. This forwarding is enabled for chrome://glic.
  test('MediaRequestAllowOnGlic', async () => {
    assertTrue(await testMediaRequest(true));
  });
  test('MediaRequestDenyOnGlic', async () => {
    assertFalse(await testMediaRequest(false));
  });
  // chrome://chrome-signin does not forward media requests in the same way, so
  // they are not allowed unless the embedded site is allowed.
  test('MediaRequestAllowOnSignIn', async () => {
    assertFalse(await testMediaRequest(true));
  });
});
