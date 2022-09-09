// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebviewManager} from 'chrome://resources/js/webview_manager.js';

// The actual URL doesn't matter since we are configuring the behavior that
// occurs before the request starts.
const TARGET_URL = 'chrome://about';

suite('WebviewManagerTest', function() {
  suiteSetup(function() {
    this.webview = document.createElement('webview');
    this.manager = new WebviewManager(this.webview);
    document.body.appendChild(this.webview);
  });

  test('WebviewManagerAccessTokenTest', async function() {
    this.manager.setAccessToken('abcdefg', (url) => {
      // Set access token on all outgoing requests.
      return true;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    this.webview.request.onSendHeaders.addListener((details) => {
      assertTrue(details.requestHeaders.includes(
          {name: 'Authorization', value: 'Bearer abcdefg'}));
    }, {urls: ['<all_urls>']}, ['requestHeaders']);
    this.webview.src = TARGET_URL;
  });

  test('WebviewManagerBlockAccessTokenTest', async function() {
    this.manager.setAccessToken('abcdefg', (url) => {
      // Set access token on all outgoing requests.
      return url === TARGET_URL;
    });
    // onSendHeaders listeners are called after onBeforeSendHeaders which are
    // used to add the token, so we use it to test the effects.
    this.webview.request.onSendHeaders.addListener((details) => {
      assertFalse(details.requestHeaders.includes(
          {name: 'Authorization', value: 'Bearer abcdefg'}));
    }, {urls: ['<all_urls>']}, ['requestHeaders']);

    // URL is not the TARGET_URL, so no token should be sent
    this.webview.src = 'chrome://version';
  });


  test('WebviewManagerAllowRequestFnTest', async function() {
    this.manager.setAllowRequestFn((url) => {
      return false;
    });

    this.webview.request.onBeforeSendHeaders.addListener((details) => {
      fail('WebviewManager should have blocked request');
    }, {urls: ['<all_urls>']}, ['requestHeaders']);

    this.webview.src = TARGET_URL;
  });
});
