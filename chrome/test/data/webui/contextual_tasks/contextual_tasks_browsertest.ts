// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

const fixtureUrl = 'chrome://webui-test/contextual_tasks/test.html';

suite('ContextualTasksAppTest', function() {
  let initialUrl: string;

  suiteSetup(() => {
    initialUrl = window.location.href;
  });

  setup(() => {
    if (initialUrl) {
      window.history.replaceState({}, '', initialUrl);
    }
  });

  test('gets thread url', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertEquals(await proxy.handler.getCallCount('getThreadUrl'), 1);
  });

  test('gets task url when query param set and updates title', async () => {
    // Set a task Uuid as a query parameter.
    const taskId = '123';
    window.history.replaceState({}, '', `?task=${taskId}`);

    // Set the q query parameter for the AI page.
    const query = 'abc';
    const fixtureUrlWithQuery = `${fixtureUrl}?q=${query}`;
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrlWithQuery);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertDeepEquals(
        await proxy.handler.whenCalled('getUrlForTask'), {value: taskId});
    assertDeepEquals(
        await proxy.handler.whenCalled('setTaskId'), {value: taskId});
    assertEquals(await proxy.handler.whenCalled('setThreadTitle'), query);

    proxy.callbackRouterRemote.setThreadTitle(query);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(document.title, query);
  });

  test('sets title to default string when query param is not set', async () => {
    // Set a task Uuid as a query parameter.
    const taskId = '123';
    window.history.replaceState({}, '', `?task=${taskId}`);

    // Don't set the q query parameter for the AI page.
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertDeepEquals(
        await proxy.handler.whenCalled('getUrlForTask'), {value: taskId});
    assertDeepEquals(
        await proxy.handler.whenCalled('setTaskId'), {value: taskId});
    assertEquals(await proxy.handler.whenCalled('setThreadTitle'), '');

    proxy.callbackRouterRemote.setThreadTitle('');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(document.title, 'AI Mode');
  });

  test('toolbar visibility changes for tab and side panel', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    // The test will start with the UI in a tab.
    proxy.handler.setIsShownInTab(true);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    assertFalse(!!appElement.shadowRoot.querySelector('top-toolbar'));

    // Now fake an event where the UI is moved to a side panel.
    proxy.handler.setIsShownInTab(false);

    proxy.callbackRouterRemote.onSidePanelStateChanged();
    proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!appElement.shadowRoot.querySelector('top-toolbar'));
  });

  test('thread url pending until oauth token', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const app = document.createElement('contextual-tasks-app');
    document.body.appendChild(app);

    await proxy.handler.whenCalled('getThreadUrl');

    const webview = app.shadowRoot.querySelector('webview');
    assertTrue(!!webview);
    assertFalse(!!webview.getAttribute('src'));

    const token = 'test_token';
    proxy.callbackRouterRemote.setOAuthToken(token);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(webview.getAttribute('src'), fixtureUrl);
  });

  test('thread url set immediately if oauth token available', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    // Delay getThreadUrl to simulate token arriving first.
    let resolveThreadUrl: (val: any) => void;
    const threadUrlPromise = new Promise<any>((resolve) => {
      resolveThreadUrl = resolve;
    });
    proxy.handler.getThreadUrl = () => {
      proxy.handler.methodCalled('getThreadUrl');
      return threadUrlPromise;
    };

    const app = document.createElement('contextual-tasks-app');
    document.body.appendChild(app);

    // Send token.
    const token = 'test_token';
    proxy.callbackRouterRemote.setOAuthToken(token);
    await proxy.callbackRouterRemote.$.flushForTesting();

    // Resolve URL.
    resolveThreadUrl!({url: {url: fixtureUrl}});
    await proxy.handler.whenCalled('getThreadUrl');
    await microtasksFinished();

    const webview = app.shadowRoot.querySelector('webview');
    assertTrue(!!webview);
    assertEquals(webview.getAttribute('src'), fixtureUrl);
  });

  test('composebox visibility toggles', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const composebox =
        appElement.shadowRoot.querySelector('contextual-tasks-composebox');
    assertTrue(!!composebox);
    assertFalse(composebox.hasAttribute('hidden'));

    // Hide the compose box.
    proxy.callbackRouterRemote.hideInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(composebox.hasAttribute('hidden'));

    // Restore the compose box.
    proxy.callbackRouterRemote.restoreInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    assertFalse(composebox.hasAttribute('hidden'));
  });

  test('task details updated in url', async () => {
    // Set the q query parameter for the AI page.
    const query = 'abc';
    const fixtureUrlWithQuery = `${fixtureUrl}?q=${query}`;
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrlWithQuery);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    const taskId = {value: '12345'};
    proxy.callbackRouterRemote.setTaskDetails(taskId, '1111', '2222');
    await proxy.callbackRouterRemote.$.flushForTesting();

    const currentUrl = new URL(window.location.href);
    assertEquals(taskId.value, currentUrl.searchParams.get('task'));
    assertEquals('1111', currentUrl.searchParams.get('thread'));
    assertEquals('2222', currentUrl.searchParams.get('turn'));
  });
});
