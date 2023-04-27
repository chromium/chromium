// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {WebviewManager} from 'chrome://parent-access/webview_manager.js';

// The actual URL doesn't matter since we are configuring the behavior that
// occurs before the request starts.
const TARGET_URL = 'https://www.chromium.org';

window.webview_manager_tests = {};
webview_manager_tests.suiteName = 'ParentAccessWebviewManagerTest';

/** @enum {string} */
webview_manager_tests.TestNames = {
  AccessTokenTest: 'AccessTokenTest',
  BlockAccessTokenTest: 'BlockAccessTokenTest',
  AllowRequestFnTest: 'AllowRequestFnTest',
};

suite(webview_manager_tests.suiteName, function() {
  let webview;
  let manager;

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    webview = document.createElement('webview');
    manager = new WebviewManager(webview);
    document.body.appendChild(webview);
  });

  test('AccessTokenTest', function(done) {
    manager.setAccessToken('abcdefg', url => {
      // Set access token on all outgoing requests.
      return true;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    webview.request.onSendHeaders.addListener(details => {
      assertTrue(details.requestHeaders.some(header => {
        return header.name === 'authorization' &&
            header.value === 'Bearer abcdefg';
      }));
      done();
    }, {urls: ['<all_urls>']}, ['requestHeaders']);
    webview.src = TARGET_URL;
  });

  test('BlockAccessTokenTest', function(done) {
    manager.setAccessToken('abcdefg', url => {
      // Set access token on all outgoing requests.
      return url === TARGET_URL;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    webview.request.onSendHeaders.addListener(details => {
      assertFalse(details.requestHeaders.some(header => {
        return header.name === 'authorization' &&
            header.value === 'Bearer abcdefg';
      }));
      done();
    }, {urls: ['<all_urls>']}, ['requestHeaders']);

    // URL is not the TARGET_URL, so no token should be sent
    webview.src = 'https://www.google.com';
  });


  test('AllowRequestFnTest', function(done) {
    manager.setAllowRequestFn(url => {
      return false;
    });

    webview.request.onBeforeSendHeaders.addListener(() => {
      fail('WebviewManager should have blocked request');
    }, {urls: ['<all_urls>']}, ['requestHeaders']);

    webview.addEventListener('loadabort', e => {
      assertEquals('ERR_BLOCKED_BY_CLIENT', e.reason);
      done();
    });

    webview.src = TARGET_URL;
  });
});
