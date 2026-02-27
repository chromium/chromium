// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

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

  test('error page shown if pending error page is true for task', async () => {
    const taskId = '123';
    window.history.replaceState({}, '', `?task=${taskId}`);

    const proxy = new TestContextualTasksBrowserProxy('http://example.com');
    proxy.handler.setIsPendingErrorPage({value: taskId}, true);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-error-page-visible_'));
  });

  test(
      'error page not shown if pending error page is not true for task',
      async () => {
        const proxy = new TestContextualTasksBrowserProxy('http://example.com');
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        assertFalse(appElement.hasAttribute('is-error-page-visible_'));
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

  test('sets basic mode when navigating from AI page and backend sends notification',
      async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Verify initial state.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'),
        'Initial state should not be in basic mode');

    // Ensure the app is on an AI page.
    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Ensure the new page is also an AI page.
    proxy.handler.setIsAiPage(true);

    // Simulate navigation start.
    const loadStartEvent = new Event('loadstart');
    Object.assign(
        loadStartEvent, {url: 'http://example.com', isTopLevel: true});
    appElement.$.threadFrame.dispatchEvent(loadStartEvent);
    await microtasksFinished();

    // Should be in basic mode now because the app is navigating from an AI
    // page.
    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    // Misc notifications do not cause z-index flickering:
    proxy.callbackRouterRemote.restoreInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    proxy.callbackRouterRemote.hideInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    proxy.callbackRouterRemote.restoreInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Simulate navigation complete. Basic mode should not be updated
    // based on the last submitted state request from the backend.
    appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
    await microtasksFinished();
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'),
        'Should change to basic mode false due to backend after navigation completes');
    assertFalse(appElement.isNavigatingForTesting(),
        'Should not be navigating after navigation completes');
  });

  test('sets basic mode as true when navigating due to backend sending notification',
      async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Verify initial state.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'),
        'Initial state should not be in basic mode');

    // Ensure the app is on an AI page.
    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Ensure the new page is also an AI page.
    proxy.handler.setIsAiPage(true);

    // Simulate navigation start.
    const loadStartEvent = new Event('loadstart');
    Object.assign(
        loadStartEvent, {url: 'http://example.com', isTopLevel: true});
    appElement.$.threadFrame.dispatchEvent(loadStartEvent);
    await microtasksFinished();

    // Should be in basic mode now because the app is navigating from an AI
    // page.
    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    // Misc notifications do not cause z-index flickering:
    proxy.callbackRouterRemote.hideInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    proxy.callbackRouterRemote.restoreInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should be in basic mode when navigating from an AI page');
    assertTrue(appElement.isNavigatingForTesting(),
        'Should be navigating after navigation starts');

    proxy.callbackRouterRemote.hideInput();
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Simulate navigation complete. Basic mode should not be updated
    // based on the last submitted state request from the backend.
    // Basic mode is true based on usage of `hideInput`.
    appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
    await microtasksFinished();
    assertTrue(appElement.hasAttribute('is-in-basic-mode_'),
        'Should change to basic mode true due to backend after navigation completes');
    assertFalse(appElement.isNavigatingForTesting(),
        'Should not be navigating after navigation completes');
  });

  test(
      'does not set basic mode when navigating from AI page to non-AI page',
      async () => {
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

        // Ensure the new page is NOT an AI page.
        proxy.handler.setIsAiPage(false);

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadStartEvent);
        await microtasksFinished();

        // Should NOT be in basic mode because we are going to a non-AI page.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
        assertFalse(appElement.isNavigatingForTesting());

        // Simulate navigation complete.
        appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
        await microtasksFinished();

        // Should still not be in basic mode.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
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
    const loadStartEvent = new Event('loadstart');
    Object.assign(
        loadStartEvent, {url: 'http://example.com', isTopLevel: true});
    appElement.$.threadFrame.dispatchEvent(loadStartEvent);
    await microtasksFinished();

    // Should NOT be in basic mode.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
    assertFalse(appElement.isNavigatingForTesting());

    // Simulate navigation complete.
    appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
    await microtasksFinished();

    // Should still not be in basic mode.
    assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
  });

  // Regression test for crbug.com/484936343.
  test(
      'restores basic mode state after navigation when starting in basic mode',
      async () => {
        // Enable the flag that forces basic mode on history.
        loadTimeData.overrideValues(
            {forceBasicModeIfOpeningThreadHistory: true});

        // Construct a URL with history params.
        const fixtureUrlWithHistory = new URL(fixtureUrl);
        fixtureUrlWithHistory.searchParams.set('atvm', '1');

        const proxy = new TestContextualTasksBrowserProxy(
            fixtureUrlWithHistory.toString());
        BrowserProxyImpl.setInstance(proxy);
        proxy.handler.setIsShownInTab(true);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        // Verify initial state is basic mode.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Ensure the app is on an AI page so navigation logic triggers.
        proxy.callbackRouterRemote.onAiPageStatusChanged(true);
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Ensure the new page is also an AI page.
        proxy.handler.setIsAiPage(true);

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadStartEvent);
        await microtasksFinished();

        // Should still be in basic mode during navigation.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
        assertTrue(appElement.isNavigatingForTesting());

        // Simulate navigation complete.
        appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
        await microtasksFinished();

        // Should still be in basic mode after navigation (restored).
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
        assertFalse(appElement.isNavigatingForTesting());
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

  test('composebox bounds update styles', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    const composebox = appElement.$.composebox;
    assertTrue(!!composebox);

    const rect = {
      top: 10,
      left: 20,
      width: 100,
      height: 200,
      right: 120,
      bottom: 210,
    };

    // Simulate callback update
    (appElement as any).forcedComposeboxBounds_ = rect;
    await microtasksFinished();

    const frameRect = appElement.$.threadFrame.getBoundingClientRect();

    // Verify styles applied
    assertEquals('relative', composebox.style.position);
    assertEquals(
        `${window.innerHeight - (frameRect.top + rect.bottom)}px`,
        composebox.style.bottom);
    assertEquals(`${frameRect.left + rect.left}px`, composebox.style.left);
    assertEquals(`${rect.width}px`, composebox.style.width);
    assertEquals('', composebox.style.height);

    // Verify zero state clears styles
    (appElement as any).isZeroState_ = true;
    await microtasksFinished();

    assertEquals('', composebox.style.position);
    assertEquals('', composebox.style.top);
    assertEquals('', composebox.style.left);
    assertEquals('', composebox.style.width);
    assertEquals('', composebox.style.height);
  });

  test('updates clip path on post message', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    // Create a promise to wait for the loadstart handler to finish. Without
    // this, forcedComposeboxBounds_ might get reset to null before the test
    // accesses it.
    const {promise, resolve} = Promise.withResolvers<void>();
    const appElement = document.createElement('contextual-tasks-app');
    appElement.setOnLoadStartFinishedCallbackForTesting(resolve);

    // Add the app element to the DOM.
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Get the webview element.
    const webview = appElement.shadowRoot.querySelector<HTMLElement>('webview');
    assertTrue(!!webview);

    // Simulate loadcommit to set up the target origin in PostMessageHandler.
    const loadCommitEvent = new Event('loadcommit');
    Object.assign(loadCommitEvent, {isTopLevel: true, url: fixtureUrl});
    webview.dispatchEvent(loadCommitEvent);

    // Wait for the loadstart handler to finish to avoid a race condition
    // between the post message setting the forcedComposeboxBounds_ and the
    // loadstart handler resetting it.
    await promise;

    // Simulate an input plate bounds update message.
    const rect = {
      top: 0,
      left: 0,
      width: 100,
      height: 100,
      right: 100,
      bottom: 100,
    };
    const occluder = {
      top: 0,
      left: 0,
      width: 50,
      height: 100,
      right: 50,
      bottom: 100,
    };
    const message = {
      type: 'input-plate-bounds-update',
      'bounds-rect': rect,
      occluders: [occluder],
    };
    window.dispatchEvent(new MessageEvent('message', {
      data: message,
      origin: new URL(fixtureUrl).origin,
    }));
    await microtasksFinished();

    // Verify properties updated
    assertDeepEquals(rect, (appElement as any).forcedComposeboxBounds_);
    assertDeepEquals([occluder], (appElement as any).occluders_);

    // Verify clip-path on webview
    const clipPath = webview.style.clipPath;
    assertTrue(
        clipPath.includes('polygon'), 'clip-path should contain polygon');
  });

  test('sets isFrameLoading to false when content load finishes', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Remove the thread frame to prevent unwanted loadstart events.
    const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
    assertTrue(!!threadFrame);
    appElement.shadowRoot.removeChild(threadFrame);
    await microtasksFinished();

    const event = new Event('loadstart');
    Object.assign(event, {url: 'http://example.com', isTopLevel: true});
    appElement.onThreadFrameLoadStartForTesting(
        event as chrome.webviewTag.LoadStartEvent);

    // Verify isFrameLoading is true.
    // Casting to any to access private property.
    assertTrue(
        appElement.getIsFrameLoadingForTesting(),
        'isFrameLoading should be true');

    // Simulate content load.
    appElement.onThreadFrameContentLoadForTesting();
    await microtasksFinished();

    // Verify isFrameLoading is false.
    assertFalse(
        appElement.getIsFrameLoadingForTesting(),
        'isFrameLoading should be false');
  });

  test('sets isFrameLoading to false when load aborts', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Remove the thread frame to prevent unwanted loadstart events.
    const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
    assertTrue(!!threadFrame);
    appElement.shadowRoot.removeChild(threadFrame);
    await microtasksFinished();

    const event = new Event('loadstart');
    Object.assign(event, {url: 'http://example.com', isTopLevel: true});
    appElement.onThreadFrameLoadStartForTesting(
        event as chrome.webviewTag.LoadStartEvent);

    // Verify isFrameLoading is true.
    assertTrue(
        appElement.getIsFrameLoadingForTesting(),
        'isFrameLoading should be true');

    // Simulate load abort.
    appElement.onThreadFrameLoadAbortForTesting();
    await microtasksFinished();

    // Verify isFrameLoading is false.
    assertFalse(
        appElement.getIsFrameLoadingForTesting(),
        'isFrameLoading should be false');
  });

  test('zero state animation plays when zero state changes', async () => {
    loadTimeData.overrideValues({
      friendlyZeroStateGaiaName: 'Test Name',
    });
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    // Set initial state to true so we can transition to false then back to
    // true.
    appElement.setIsZeroStateForTesting(true);
    await microtasksFinished();

    const composebox = appElement.$.composebox;
    const headerWrapper = appElement.$.composeboxHeaderWrapper;
    // nameShimmer might not exist if friendlyZeroStateGaiaName_ is not set.
    const nameShimmer = appElement.$.nameShimmer;

    // Mock animate function.
    let composeboxAnimateCalled = false;
    let headerAnimateCalled = false;
    let nameShimmerAnimateCalled = false;

    // Mock getAnimations to return dummy animations that can be cancelled and
    // played.
    const createMockAnimation = (callback: () => void) =>
        ({
          cancel: () => {},
          play: () => {
            callback();
            return Promise.resolve();
          },
        }) as unknown as Animation;

    composebox.getAnimations = () => [createMockAnimation(() => {
      composeboxAnimateCalled = true;
    })];

    headerWrapper.getAnimations = () => [createMockAnimation(() => {
      headerAnimateCalled = true;
    })];

    if (nameShimmer) {
      nameShimmer.getAnimations = () => [createMockAnimation(() => {
        nameShimmerAnimateCalled = true;
      })];
    }

    // Mock startExpandAnimation since it is called to trigger the glow
    // animation.
    (composebox as any).startExpandAnimation = () => {};

    // Transition out of zero state first.
    proxy.callbackRouterRemote.onZeroStateChange(false);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Transition back to zero state to trigger animations.
    proxy.callbackRouterRemote.onZeroStateChange(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Verify animations were played.
    assertTrue(composeboxAnimateCalled, 'Composebox animation should play');
    assertTrue(headerAnimateCalled, 'Header animation should play');
    if (nameShimmer) {
      assertTrue(
          nameShimmerAnimateCalled, 'Name shimmer animation should play');
    }
  });

  test(
      'sets pending basic mode to false when navigating from AI page and initially not in basic mode',
      async () => {
        loadTimeData.overrideValues({enableBasicMode: true});
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

        // Ensure the new page is also an AI page.
        proxy.handler.setIsAiPage(true);

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadStartEvent);
        await microtasksFinished();

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Verify pendingBasicMode_ is false (private property access).
        assertFalse((appElement as any).pendingBasicMode_);

        // Simulate navigation complete.
        appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
        await microtasksFinished();

        // Should exit basic mode because pendingBasicMode_ was false.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
      });

  test(
      'does not set pending basic mode when navigating from AI page and initially in basic mode',
      async () => {
        loadTimeData.overrideValues({enableBasicMode: true});
        const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        // Force into basic mode initially.
        proxy.callbackRouterRemote.hideInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Ensure the app is on an AI page.
        proxy.callbackRouterRemote.onAiPageStatusChanged(true);
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Ensure the new page is also an AI page.
        proxy.handler.setIsAiPage(true);

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadStartEvent);
        await microtasksFinished();

        // Verify pendingBasicMode_ is null (private property access).
        assertEquals(null, (appElement as any).pendingBasicMode_);

        // Simulate navigation complete.
        appElement.$.threadFrame.dispatchEvent(new Event('contentload'));
        await microtasksFinished();

        // Should remain in basic mode because pendingBasicMode_ was null.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
      });

  test(
      'updates basic mode on load commit when navigating from AI page and initially not in basic mode',
      async () => {
        loadTimeData.overrideValues({enableBasicMode: true});
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

        // Ensure the new page is also an AI page.
        proxy.handler.setIsAiPage(true);

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadStartEvent);
        await microtasksFinished();

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Verify pendingBasicMode_ is false (private property access).
        assertFalse((appElement as any).pendingBasicMode_);

        // Simulate load commit.
        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.$.threadFrame.dispatchEvent(loadCommitEvent);
        await microtasksFinished();

        // Should exit basic mode because pendingBasicMode_ was false.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
      });
});
