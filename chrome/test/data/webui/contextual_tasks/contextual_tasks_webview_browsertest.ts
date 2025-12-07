// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

const fixtureUrl = 'chrome://webui-test/contextual_tasks/test.html';

suite('ContextualTasksWebviewTest', function() {
  test('webview adds oauth token to request headers', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    // Wait for app to finish initializing, which includes setting the initial
    // webview URL.
    await microtasksFinished();

    // Get the webview element.
    const threadFrame =
        appElement.shadowRoot.querySelector<chrome.webviewTag.WebView>(
            '#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    // Simulate the browser pushing the OAuth token to the page.
    proxy.callbackRouterRemote.setOAuthToken('fake_token');
    await microtasksFinished();

    // Add a promise that will be resolved after the headers contain the OAuth
    // token.
    const headersPromise =
        new Promise<chrome.webRequest.HttpHeaders|undefined>(resolve => {
          const listener = (details: any) => {
            // Remove listener to avoid capturing other requests.
            threadFrame.request.onSendHeaders.removeListener(listener);
            resolve(details.requestHeaders);
          };

          threadFrame.request.onSendHeaders.addListener(
              listener, {urls: ['<all_urls>']}, ['requestHeaders']);
        });

    // Switch the URL to trigger a request. Note, this needs to a real URL or
    // else the request will not actually trigger the listener. However, this
    // does not actually load a URL in the webview, because browsertests use a
    // mock server implementation.
    threadFrame.src = 'https://www.google.com';

    // Verify that the OAuth token was added to the request headers.
    const headers = await headersPromise;
    assertTrue(!!headers, 'Request headers not found');
    const authHeader =
        headers.find(h => h.name.toLowerCase() === 'authorization');
    assertTrue(!!authHeader, 'Authorization header not found');
    assertEquals(`Bearer fake_token`, authHeader.value);
  });

  test('webview adds user agent to request headers', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    // Wait for app to finish initializing, which includes setting the initial
    // webview URL.
    await microtasksFinished();

    // Get the webview element.
    const threadFrame =
        appElement.shadowRoot.querySelector<chrome.webviewTag.WebView>(
            '#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    // Add a promise that will be resolved after the headers contain the OAuth
    // token.
    const headersPromise =
        new Promise<chrome.webRequest.HttpHeaders|undefined>(resolve => {
          const listener = (details: any) => {
            // Remove listener to avoid capturing other requests.
            threadFrame.request.onSendHeaders.removeListener(listener);
            resolve(details.requestHeaders);
          };

          threadFrame.request.onSendHeaders.addListener(
              listener, {urls: ['<all_urls>']}, ['requestHeaders']);
        });

    // Switch the URL to trigger a request. Note, this needs to a real URL or
    // else the request will not actually trigger the listener. However, this
    // does not actually load a URL in the webview, because browsertests use a
    // mock server implementation.
    threadFrame.src = 'https://www.google.com';

    // Verify that the OAuth token was added to the request headers.
    const headers = await headersPromise;
    assertTrue(!!headers, 'Request headers not found');
    const userAgentHeader =
        headers.find(h => h.name.toLowerCase() === 'user-agent');
    assertTrue(!!userAgentHeader, 'User-Agent header not found');
    assertTrue(!!userAgentHeader.value, 'User-Agent header value is empty');
    assertTrue(
        userAgentHeader.value.includes('WGA/'),
        'User-Agent header does not contain our custom user agent');
  });
});
