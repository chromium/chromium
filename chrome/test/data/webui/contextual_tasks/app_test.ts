// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {OnBeforeRequestDetails} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    if (initialUrl) {
      window.history.replaceState({}, '', initialUrl);
    }
    loadTimeData.overrideValues({enableBasicModeZOrder: true});
  });

  test('gets thread url', () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertEquals(1, proxy.handler.getCallCount('getThreadUrl'));
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
        {value: taskId}, await proxy.handler.whenCalled('getUrlForTask'));
    assertDeepEquals(
        {value: taskId}, await proxy.handler.whenCalled('setTaskId'));
    assertEquals(query, await proxy.handler.whenCalled('setThreadTitle'));

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
        {value: taskId}, await proxy.handler.whenCalled('getUrlForTask'));
    assertDeepEquals(
        {value: taskId}, await proxy.handler.whenCalled('setTaskId'));
    assertEquals('', await proxy.handler.whenCalled('setThreadTitle'));

    proxy.callbackRouterRemote.setThreadTitle('');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals('AI Mode', document.title);
  });

  test('restores thread url with webui url params', async () => {
    const taskId = '123';
    const threadId = '111';
    const turnId = '222';
    const title = 'title';
    window.history.replaceState(
        {}, '',
        `?task=${taskId}&thread=${threadId}&turn=${turnId}&title=${title}`);

    // Don't set the q query parameter for the AI page.
    const proxy = new TestContextualTasksBrowserProxy('http://example.com');
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadUrl = new URL(appElement.getThreadUrlForTesting());

    assertEquals(threadId, threadUrl.searchParams.get('mtid'));
    assertEquals(turnId, threadUrl.searchParams.get('mstk'));
    assertEquals(title, threadUrl.searchParams.get('q'));
  });

  test('does not attempt to restore thread if params available', async () => {
    window.history.replaceState(
        {}, '', `?task=123&thread=333&turn=444&title=wrong`);

    const threadId = '111';
    const turnId = '222';
    const title = 'title';
    const proxy = new TestContextualTasksBrowserProxy(
        `http://example.com?mtid=${threadId}&mstk=${turnId}&q=${title}`);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadUrl = new URL(appElement.getThreadUrlForTesting());

    assertEquals(threadId, threadUrl.searchParams.get('mtid'));
    assertEquals(turnId, threadUrl.searchParams.get('mstk'));
    assertEquals(title, threadUrl.searchParams.get('q'));
  });

  test('history entry added if task changes', async () => {
    window.history.replaceState(
        {}, '', `?task=111&thread=222&turn=333&title=wrong`);

    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const initialHistoryLength = window.history.length;

    // Since the task ID is different from the one above, this call should add
    // an entry to history.
    proxy.callbackRouterRemote.setTaskDetails({value: '123'}, '456', '789');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(initialHistoryLength + 1, window.history.length);
  });

  test('no history entry added if task did not change', async () => {
    window.history.replaceState(
        {}, '', `?task=111&thread=222&turn=333&title=wrong`);

    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const initialHistoryLength = window.history.length;

    // Since the task ID is is the same as above, a history entry should not be
    // added.
    proxy.callbackRouterRemote.setTaskDetails({value: '111'}, '456', '789');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(initialHistoryLength, window.history.length);
  });

  test('back navigation fetches previous task url', async () => {
    window.history.replaceState(
        {}, '', `?task=111&thread=222&turn=333&title=wrong`);

    const proxy = new TestContextualTasksBrowserProxy(
        `http://example.com?mtid=111&mstk=222&q=title`);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    const {promise, resolve} = Promise.withResolvers<void>();
    appElement.setPopStateFinishedCallbackForTesting(resolve);
    await microtasksFinished();

    // Fake a task change event.
    proxy.callbackRouterRemote.setTaskDetails({value: '999'}, '456', '789');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    window.history.back();

    // There should have been a call to get the url for the previous task.
    assertDeepEquals(
        {value: '111'}, await proxy.handler.whenCalled('getUrlForTask'));

    await promise;
  });

  test('history requested if url param set', async () => {
    // Make sure the history panel is requested in the URL.
    window.history.replaceState({}, '', `?open_history=true`);

    const proxy = new TestContextualTasksBrowserProxy('http://example.com');
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const threadUrl = new URL(appElement.getThreadUrlForTesting());

    // The param to open history should have been added to the initial thread
    // URL.
    assertEquals('1', threadUrl.searchParams.get('atvm'));

    // The URL param asking to open history should have been removed.
    const currentUrl = new URL(window.location.href);
    assertFalse(currentUrl.searchParams.has('open_history'));

    await microtasksFinished();
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

  test('thread url set immediately', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const app = document.createElement('contextual-tasks-app');
    document.body.appendChild(app);

    await proxy.handler.whenCalled('getThreadUrl');
    await microtasksFinished();

    const webview = app.shadowRoot.querySelector('webview');
    assertTrue(!!webview);
    assertEquals(fixtureUrl, webview.getAttribute('src'));
  });


  test(
      'composebox z-index changes when visibility toggles with enableBasicModeZOrder',
      async () => {
        const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        const composebox =
            appElement.shadowRoot.querySelector('contextual-tasks-composebox');
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        const flexCenterContainer =
            appElement.shadowRoot.querySelector('#flexCenterContainer');

        assertTrue(!!composebox);
        assertFalse(composebox.hasAttribute('hidden'));

        // Hide the compose box (enter basic mode).
        proxy.callbackRouterRemote.hideInput();
        await proxy.callbackRouterRemote.$.flushForTesting();

        // With flag enabled, hidden attribute should NOT be present.
        assertFalse(composebox.hasAttribute('hidden'));

        const threadFrameStyle = getComputedStyle(threadFrame!);
        const flexCenterStyle = getComputedStyle(flexCenterContainer!);

        assertEquals(
            '1', threadFrameStyle.zIndex, 'Thread frame z-index should be 1');
        assertEquals(
            '0', flexCenterStyle.zIndex,
            'Flex center container z-index should be 0');

        // Restore the compose box.
        proxy.callbackRouterRemote.restoreInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        assertFalse(composebox.hasAttribute('hidden'));

        const threadFrameStyleRestored = getComputedStyle(threadFrame!);
        const flexCenterStyleRestored = getComputedStyle(flexCenterContainer!);

        // Verify z-index is not stuck
        assertFalse(threadFrameStyleRestored.zIndex === '1');
        assertFalse(flexCenterStyleRestored.zIndex === '0');
      });

  test(
      'composebox visibility toggles with enableBasicModeZOrder set to false',
      async () => {
        loadTimeData.overrideValues({enableBasicModeZOrder: false});
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

  test('aim url saved in contextual task url', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    const aimUrl = 'https://www.google.com/search?q=123';
    proxy.callbackRouterRemote.setAimUrl(aimUrl);
    await proxy.callbackRouterRemote.$.flushForTesting();

    const currentUrl = new URL(window.location.href);
    assertEquals(aimUrl, currentUrl.searchParams.get('aim_url'));
  });

  test('isAiPage reflected in dom', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-ai-page_'));

    proxy.callbackRouterRemote.onAiPageStatusChanged(false);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertFalse(appElement.hasAttribute('is-ai-page_'));

    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-ai-page_'));
  });

  // Disabled due to flakiness. See http://crbug.com/481936603.
  test.skip('copies source and aep params on new thread click', async () => {
    const initialThreadUrl = new URL('http://example.com?q=initial');
    initialThreadUrl.searchParams.set('source', 'some-source');
    initialThreadUrl.searchParams.set('aep', 'some-aep');

    const proxy = new TestContextualTasksBrowserProxy(initialThreadUrl.href);
    BrowserProxyImpl.setInstance(proxy);
    proxy.handler.setIsShownInTab(true);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Switch to side panel view, which should show the toolbar.
    proxy.handler.setIsShownInTab(false);
    proxy.callbackRouterRemote.onSidePanelStateChanged();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Make sure the initial URL is set.
    assertEquals(initialThreadUrl.href, appElement.getThreadUrlForTesting());

    const newThreadUrl = 'http://new-thread.com/';
    proxy.handler.setThreadUrl(newThreadUrl);

    // Simulate a new thread click from the toolbar.
    const toolbar = appElement.shadowRoot.querySelector('top-toolbar');
    assertTrue(!!toolbar, 'Toolbar should be visible');
    toolbar.dispatchEvent(
        new CustomEvent('new-thread-click', {bubbles: true, composed: true}));
    await microtasksFinished();

    const finalUrl = new URL(appElement.getThreadUrlForTesting());
    assertEquals(newThreadUrl, finalUrl.origin + finalUrl.pathname);
    assertEquals('some-source', finalUrl.searchParams.get('source'));
    assertEquals('some-aep', finalUrl.searchParams.get('aep'));
  });

  test(
      'does not force enter basic mode when thread history is open if flag is disabled',
      async () => {
        loadTimeData.overrideValues(
            {forceBasicModeIfOpeningThreadHistory: false});
        const fixtureUrlWithHistory = new URL(fixtureUrl);
        fixtureUrlWithHistory.searchParams.set('atvm', '1');
        const proxy = new TestContextualTasksBrowserProxy(
            fixtureUrlWithHistory.toString());
        BrowserProxyImpl.setInstance(proxy);
        proxy.handler.setIsShownInTab(true);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        const composebox =
            appElement.shadowRoot.querySelector('contextual-tasks-composebox');
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        const flexCenterContainer =
            appElement.shadowRoot.querySelector('#flexCenterContainer');

        assertTrue(!!composebox);
        assertFalse(composebox.hasAttribute('hidden'));

        const threadFrameStyle = getComputedStyle(threadFrame!);
        const flexCenterStyle = getComputedStyle(flexCenterContainer!);

        // Verify z-index is not set to basic mode values
        assertFalse(threadFrameStyle.zIndex === '1');
        assertFalse(flexCenterStyle.zIndex === '0');
      });

  test(
      'force enter basic mode when thread URL has history params', async () => {
        loadTimeData.overrideValues(
            {forceBasicModeIfOpeningThreadHistory: true});
        const fixtureUrlWithHistory = new URL(fixtureUrl);
        fixtureUrlWithHistory.searchParams.set('atvm', '1');
        const proxy = new TestContextualTasksBrowserProxy(
            fixtureUrlWithHistory.toString());
        BrowserProxyImpl.setInstance(proxy);
        proxy.handler.setIsShownInTab(true);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        const composebox =
            appElement.shadowRoot.querySelector('contextual-tasks-composebox');
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        const flexCenterContainer =
            appElement.shadowRoot.querySelector('#flexCenterContainer');

        assertTrue(!!composebox);
        // With z-order enabled, it should NOT be hidden
        assertFalse(composebox.hasAttribute('hidden'));

        const threadFrameStyle = getComputedStyle(threadFrame!);
        const flexCenterStyle = getComputedStyle(flexCenterContainer!);

        assertEquals(
            '1', threadFrameStyle.zIndex, 'Thread frame z-index should be 1');
        assertEquals(
            '0', flexCenterStyle.zIndex,
            'Flex center container z-index should be 0');
      });

  test('sets basic mode when navigating from AI page', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Verify initial state.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));

    // Ensure the app is on an AI page.
    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Simulate navigation start.
    appElement.onBeforeRequestForTesting(
        {url: 'http://example.com'} as OnBeforeRequestDetails);
    await microtasksFinished();

    // Should be in basic mode now because the app is navigating from an AI page.
    assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
    assertTrue(appElement.isNavigatingForTesting());

    // Simulate navigation complete.
    appElement.onCompletedForTesting();
    await microtasksFinished();

    // Should exit basic mode.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
    assertFalse(appElement.isNavigatingForTesting());
  });

  test('does not set basic mode when navigating from non-AI page', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Verify initial state.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));

    // Ensure the app is NOT on an AI page.
    proxy.callbackRouterRemote.onAiPageStatusChanged(false);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Simulate navigation start.
    appElement.onBeforeRequestForTesting(
        {url: 'http://example.com'} as OnBeforeRequestDetails);
    await microtasksFinished();

    // Should NOT be in basic mode.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
    assertFalse(appElement.isNavigatingForTesting());

    // Simulate navigation complete.
    appElement.onCompletedForTesting();
    await microtasksFinished();

    // Should still not be in basic mode.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
  });

  test('sends composebox height update', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Mock the post message handler to verify that the composebox height update
    // is sent.
    let sentMessage: any = null;
    const mockPostMessageHandler = {
      sendObjectMessage: (message: any) => {
        sentMessage = message;
      },
      completeHandshake: () => {},
      sendMessage: () => {},
    };
    appElement.setMockPostMessageHandlerForTesting(
        mockPostMessageHandler as any);

    // Set the height of the composebox to 123px.
    const composebox =
        appElement.shadowRoot.querySelector('contextual-tasks-composebox');
    assertTrue(!!composebox);
    const innerComposebox =
        composebox.shadowRoot.querySelector<HTMLElement>('#composebox');
    assertTrue(!!innerComposebox);
    innerComposebox.style.height = '123px';

    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Verify that the new composebox height is sent to the webview.
    assertDeepEquals(
        {type: 'composebox-height-update', height: 123}, sentMessage);
  });

  test(
      'lockInput and unlockInput updates composebox inputEnabled', async () => {
        const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        const composebox = appElement.$.composebox;
        assertTrue(composebox.inputEnabled);

        proxy.callbackRouterRemote.lockInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertFalse(composebox.inputEnabled);

        proxy.callbackRouterRemote.unlockInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertTrue(composebox.inputEnabled);
      });
});
