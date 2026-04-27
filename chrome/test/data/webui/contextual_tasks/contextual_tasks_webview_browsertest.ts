// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {isFullWebView} from 'chrome://contextual-tasks/web_view_type.js';
import type {WebViewType} from 'chrome://contextual-tasks/web_view_type.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {fixtureUrl} from './test_utils.js';

suite('ContextualTasksWebviewTest', function() {


  test('webview does not add gsc param when in tab', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    proxy.handler.setIsShownInTab(true);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadFrame =
        appElement.shadowRoot.querySelector<WebViewType>('#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    if (!isFullWebView(threadFrame)) {
      return;
    }

    const completionPromise = new Promise<void>(resolve => {
      const listener = (details: any) => {
        threadFrame.request.onBeforeSendHeaders.removeListener(listener);
        const url = new URL(details.url);
        assertEquals(
            null, url.searchParams.get('gsc'), 'gsc param should not be set');
        resolve();
        return {};
      };

      threadFrame.request.onBeforeSendHeaders.addListener(
          listener, {urls: ['<all_urls>']}, ['requestHeaders']);
    });

    threadFrame.src = 'https://www.google.com/';
    await completionPromise;
  });

  test('webview preserves gsc param if explicitly in url', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    proxy.handler.setIsShownInTab(true);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadFrame =
        appElement.shadowRoot.querySelector<WebViewType>('#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    if (!isFullWebView(threadFrame)) {
      return;
    }

    const completionPromise = new Promise<void>(resolve => {
      const listener = (details: any) => {
        threadFrame.request.onBeforeSendHeaders.removeListener(listener);
        const url = new URL(details.url);
        assertEquals(
            '3', url.searchParams.get('gsc'), 'gsc param should be preserved');
        resolve();
        return {};
      };

      threadFrame.request.onBeforeSendHeaders.addListener(
          listener, {urls: ['<all_urls>']}, ['requestHeaders']);
    });

    threadFrame.src = 'https://www.google.com/?gsc=3';
    await completionPromise;
  });

  test('webview adds gsc param when in side panel', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    proxy.handler.setIsShownInTab(false);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadFrame =
        appElement.shadowRoot.querySelector<WebViewType>('#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    if (!isFullWebView(threadFrame)) {
      return;
    }

    const completionPromise = new Promise<void>(resolve => {
      const listener = (details: any) => {
        threadFrame.request.onBeforeSendHeaders.removeListener(listener);
        const url = new URL(details.url);
        assertEquals('2', url.searchParams.get('gsc'));
        resolve();
        return {};
      };

      threadFrame.request.onBeforeSendHeaders.addListener(
          listener, {urls: ['<all_urls>']}, ['requestHeaders']);
    });

    threadFrame.src = 'https://www.google.com/';
    await completionPromise;
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
        appElement.shadowRoot.querySelector<WebViewType>('#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    if (!isFullWebView(threadFrame)) {
      return;
    }

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
        userAgentHeader.value.includes('Cobrowsing/'),
        'User-Agent header does not contain our custom user agent');
  });

  test('webview replaces host when set', async () => {
    loadTimeData.overrideValues({chrome_host: 'corp.google.com'});

    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    proxy.handler.setIsShownInTab(true);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadFrame = appElement.$.threadFrame as chrome.webviewTag.WebView;
    assertTrue(!!threadFrame, 'Thread frame not found');

    const completionPromise = new Promise<void>(resolve => {
      const listener = (details: any) => {
        threadFrame.request.onBeforeSendHeaders.removeListener(listener);
        const url = new URL(details.url);
        assertEquals('corp.google.com', url.host);
        resolve();
        return {};
      };

      threadFrame.request.onBeforeSendHeaders.addListener(
          listener, {urls: ['<all_urls>']}, ['requestHeaders']);
    });

    threadFrame.src = 'https://www.google.com/';
    await completionPromise;
  });

  test('webview adds viewport size params', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    // Set a size to ensure getBoundingClientRect returns non-zero values.
    appElement.style.display = 'block';
    appElement.style.width = '800px';
    appElement.style.height = '600px';
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadFrame =
        appElement.shadowRoot.querySelector<WebViewType>('#threadFrame');
    assertTrue(!!threadFrame, 'Thread frame not found');

    if (!isFullWebView(threadFrame)) {
      return;
    }

    const completionPromise = new Promise<void>(resolve => {
      const listener = (details: any) => {
        threadFrame.request.onBeforeSendHeaders.removeListener(listener);
        const url = new URL(details.url);
        assertTrue(url.searchParams.has('biw'), 'biw param should be set');
        assertTrue(url.searchParams.has('bih'), 'bih param should be set');
        assertTrue(
            parseInt(url.searchParams.get('biw')!) > 0,
            'biw should be greater than 0');
        assertTrue(
            parseInt(url.searchParams.get('bih')!) > 0,
            'bih should be greater than 0');
        resolve();
        return {};
      };

      threadFrame.request.onBeforeSendHeaders.addListener(
          listener, {urls: ['<all_urls>']}, ['requestHeaders']);
    });

    threadFrame.src = 'https://www.google.com/';
    await completionPromise;
  });
});
