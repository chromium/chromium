// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {WebviewManager} from 'chrome://parent-access/webview_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {clearDocumentBody} from './parent_access_test_utils.js';

interface OnSendHeadersDetails {
  requestHeaders: chrome.webRequest.HttpHeaders;
}

const TARGET_URL = 'https://www.chromium.org';

suite('ParentAccessWebviewManagerTest', function() {
  let webview: chrome.webviewTag.WebView;
  let manager: WebviewManager;

  setup(function() {
    clearDocumentBody();
    webview = document.createElement('webview') as chrome.webviewTag.WebView;
    manager = new WebviewManager(webview);
    document.body.appendChild(webview);
  });

  // Checks that the access token is set on outgoing requests for the target
  // URL.
  test('AccessTokenTest', function(done) {
    manager.setAccessToken('abcdefg', () => {
      // Set access token on all outgoing requests.
      return true;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    webview.request.onSendHeaders.addListener(
        (details: OnSendHeadersDetails) => {
          assertTrue(details.requestHeaders.some((header) => {
            return header.name === 'authorization' &&
                header.value === 'Bearer abcdefg';
          }));
          done();
        },
        {urls: ['<all_urls>']}, ['requestHeaders']);
    webview.src = TARGET_URL;
  });

  // Checks that the access token is not set for URLs that are not the target
  // URL.
  test('BlockAccessTokenTest', function(done) {
    manager.setAccessToken('abcdefg', url => {
      // Set access token on all outgoing requests.
      return url === TARGET_URL;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    webview.request.onSendHeaders.addListener(
        (details: OnSendHeadersDetails) => {
          assertFalse(details.requestHeaders.some((header) => {
            return header.name === 'authorization' &&
                header.value === 'Bearer abcdefg';
          }));
          done();
        },
        {urls: ['<all_urls>']}, ['requestHeaders']);

    // URL is not the TARGET_URL, so no token should be sent
    webview.src = 'https://www.google.com';
  });


  test('AllowRequestFnTest', function(done) {
    manager.setAllowRequestFn(() => {
      return false;
    });

    webview.request.onBeforeSendHeaders.addListener(() => {
      chrome.test.fail('WebviewManager should have blocked request');
    }, {urls: ['<all_urls>']}, ['requestHeaders']);

    webview.addEventListener('loadabort', (e: any) => {
      assertEquals('ERR_BLOCKED_BY_CLIENT', e.reason);
      done();
    });

    webview.src = TARGET_URL;
  });
});
