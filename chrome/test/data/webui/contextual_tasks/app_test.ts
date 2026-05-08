// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {createContextualTasksAppElement, fixtureUrl} from './test_utils.js';

// Remove the element to prevent background loadabort events from triggering
// a race condition with our manual event simulation.
async function removeThreadFrameToPreventRaceConditions() {
  const appElement = document.querySelector('contextual-tasks-app');
  const threadFrame =
      appElement?.shadowRoot.querySelector<HTMLElement>('#threadFrame');

  if (threadFrame && isVisible(threadFrame)) {
    threadFrame.remove();
    await microtasksFinished();
  }
}

suite('ContextualTasksAppTest', function() {
  let initialUrl: string;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    initialUrl = window.location.href;
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    if (initialUrl) {
      window.history.replaceState({}, '', initialUrl);
    }
    loadTimeData.overrideValues({
      enableBasicModeZOrder: true,
      enableComposeboxJumpFix: false,
      isGhostLoaderVisible: false,
      isAiPage: true,
      nlmUrlParam: 'ajid',
      enableCustomNlmUi: true,
    });
    metrics = fakeMetricsPrivate();
    const proxy = new TestContextualTasksBrowserProxy('http://example.com');
    BrowserProxyImpl.setInstance(proxy);
  });

  test('gets thread url', async () => {
    const {proxy} = await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    assertEquals(1, proxy.handler.getCallCount('getThreadUrl'));
  });

  test('gets task url when query param set and updates title', async () => {
    // Set a task Uuid as a query parameter.
    const taskId = '123';
    window.history.replaceState({}, '', `?chrome_task_id=${taskId}`);

    // Set the q query parameter for the AI page.
    const query = 'abc';
    const fixtureUrlWithQuery = `${fixtureUrl}?q=${query}`;
    const {proxy} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrlWithQuery);

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
    window.history.replaceState({}, '', `?chrome_task_id=${taskId}`);

    // Don't set the q query parameter for the AI page.
    const {proxy} = await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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

  test('properties initialized from loadTimeData', async () => {
    loadTimeData.overrideValues({
      isAiPage: false,
      isZeroState: false,
    });

    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    assertFalse(appElement.hasAttribute('is-ai-page_'));
    assertFalse(appElement.hasAttribute('is-zero-state_'));
  });

  test('host initialized from URL parameter', async () => {
    const forcedHost = 'test.host.com';
    window.history.replaceState({}, '', `?chrome_host=${forcedHost}`);

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    assertEquals(forcedHost, (appElement as any).host_);
  });

  test('host initialized from loadTimeData when URL param absent', async () => {
    const forcedHost = 'default.host.com';
    loadTimeData.overrideValues({chrome_host: forcedHost});

    const appElement = document.createElement('contextual-tasks-app');
    document.body.appendChild(appElement);
    await microtasksFinished();

    assertEquals(forcedHost, (appElement as any).host_);
  });


  test('restores thread if task param set', async () => {
    window.history.replaceState({}, '', '?chrome_task_id=123');

    // Don't set the q query parameter for the AI page.
    const {proxy} =
        await createContextualTasksAppElement(/*url=*/ 'http://example.com');

    assertEquals(1, proxy.handler.getCallCount('getUrlForTask'));
  });

  test('does not attempt to restore thread if params available', async () => {
    window.history.replaceState(
        {}, '', `?chrome_task_id=123&thread=333&turn=444&title=wrong`);

    const threadId = '111';
    const turnId = '222';
    const title = 'title';
    const {appElement} = await createContextualTasksAppElement(
        /*url=*/ `http://example.com?mtid=${threadId}&mstk=${turnId}&q=${
            title}`);

    const threadUrl = new URL(appElement.getThreadUrlForTesting());

    assertEquals(threadId, threadUrl.searchParams.get('mtid'));
    assertEquals(turnId, threadUrl.searchParams.get('mstk'));
    assertEquals(title, threadUrl.searchParams.get('q'));
  });

  test('history entry added if task changes', async () => {
    window.history.replaceState(
        {}, '', `?chrome_task_id=111&thread=222&turn=333&title=wrong`);

    const {proxy} = await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    const initialHistoryLength = window.history.length;

    // Since the task ID is different from the one above, this call should add
    // an entry to history.
    proxy.callbackRouterRemote.setTaskDetails({value: '123'});
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(initialHistoryLength + 1, window.history.length);
  });

  test('no history entry added if task did not change', async () => {
    window.history.replaceState(
        {}, '', `?chrome_task_id=111&thread=222&turn=333&title=wrong`);

    const {proxy} = await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    const initialHistoryLength = window.history.length;

    // Since the task ID is is the same as above, a history entry should not be
    // added.
    proxy.callbackRouterRemote.setTaskDetails({value: '111'});
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(initialHistoryLength, window.history.length);
  });

  test('back navigation fetches previous task url', async () => {
    window.history.replaceState(
        {}, '', `?chrome_task_id=111&thread=222&turn=333&title=wrong`);

    const {appElement, proxy} = await createContextualTasksAppElement(
        /*url=*/ `http://example.com?mtid=111&mstk=222&q=title`);
    const {promise, resolve} = Promise.withResolvers<void>();
    appElement.setPopStateFinishedCallbackForTesting(resolve);
    await microtasksFinished();

    // Fake a task change event.
    proxy.callbackRouterRemote.setTaskDetails({value: '999'});
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

    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ 'http://example.com');

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
    window.history.replaceState({}, '', `?chrome_task_id=${taskId}`);

    const {appElement} = await createContextualTasksAppElement(
        /*url=*/ 'http://example.com',
        /*setupProxy=*/
        (p) => {
          p.handler.setIsPendingErrorPage({value: taskId}, true);
        },
        /*waitForInitialLoadStart=*/ false);

    assertTrue(appElement.hasAttribute('is-error-page-visible_'));
  });

  test(
      'error page not shown if pending error page is not true for task',
      async () => {
        const {appElement} = await createContextualTasksAppElement(
            /*url=*/ 'http://example.com');

        assertFalse(appElement.hasAttribute('is-error-page-visible_'));
      });

  test('toolbar visibility changes for tab and side panel', async () => {
    const {appElement, proxy} = await createContextualTasksAppElement(
        /*url=*/ fixtureUrl,
        /*setupProxy=*/ (p) => {
          p.handler.setIsShownInTab(true);
        });

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

  test('task details updated in url', async () => {
    // Set the q query parameter for the AI page.
    const query = 'abc';
    const fixtureUrlWithQuery = `${fixtureUrl}?q=${query}`;
    const {proxy} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrlWithQuery);

    const taskId = {value: '12345'};
    proxy.callbackRouterRemote.setTaskDetails(taskId);
    await proxy.callbackRouterRemote.$.flushForTesting();

    const currentUrl = new URL(window.location.href);
    assertEquals(taskId.value, currentUrl.searchParams.get('chrome_task_id'));
  });

  test('aim url updates webui url params', async () => {
    const {proxy} = await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    const aimUrl = `${fixtureUrl}/search?q=123&mtid=456&old_param=1`;
    proxy.callbackRouterRemote.setAimUrl(aimUrl);
    await proxy.callbackRouterRemote.$.flushForTesting();

    let currentUrl = new URL(window.location.href);
    assertEquals('123', currentUrl.searchParams.get('q'));
    assertEquals('456', currentUrl.searchParams.get('mtid'));
    assertEquals('1', currentUrl.searchParams.get('old_param'));

    // Ensure old params are removed if no longer present on the aim URL.
    const updatedAimUrl = `${fixtureUrl}/search?q=123&mtid=456&new_param=2`;
    proxy.callbackRouterRemote.setAimUrl(updatedAimUrl);
    await proxy.callbackRouterRemote.$.flushForTesting();

    currentUrl = new URL(window.location.href);
    assertEquals('123', currentUrl.searchParams.get('q'));
    assertEquals('456', currentUrl.searchParams.get('mtid'));
    assertEquals('2', currentUrl.searchParams.get('new_param'));
    assertFalse(currentUrl.searchParams.has('old_param'));
  });

  // Disabled: crbug.com/507859340
  test.skip('cs param updates dark mode only on commit', async () => {
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);
    // Initial state should be light mode (or whatever default is).
    assertFalse(appElement['darkMode_']);
    const urlWithCs1 = `${fixtureUrl}?cs=1`;
    // 1. Test that loadstart alone does NOT update theme.
    const eventStart = {
      url: urlWithCs1,
      isTopLevel: true,
    } as unknown as chrome.webviewTag.LoadStartEvent;
    appElement.onThreadFrameLoadStartForTesting(eventStart);
    await microtasksFinished();
    // Should still be false because logic moved to
    // maybeOnThreadFrameTopLevelNavigation which is called on commit/redirect.
    assertFalse(appElement['darkMode_']);
    // 2. Test that loadabort prevents update.
    const eventAbort = {
      url: urlWithCs1,
      isTopLevel: true,
    } as unknown as chrome.webviewTag.LoadAbortEvent;
    await appElement.onThreadFrameLoadAbortForTesting(eventAbort);
    await microtasksFinished();
    assertFalse(appElement['darkMode_']);
    // 3. Test that loadcommit updates theme.
    // Need to call loadstart again to set lastThreadFrameLoadStartEvent_
    appElement.onThreadFrameLoadStartForTesting(eventStart);
    await microtasksFinished();
    const eventCommit = {
      url: urlWithCs1,
      isTopLevel: true,
    } as unknown as chrome.webviewTag.LoadCommitEvent;
    appElement.onThreadFrameLoadCommitForTesting(eventCommit);
    await microtasksFinished();
    assertTrue(appElement['darkMode_']);
  });
  test('isAiPage reflected in dom', async () => {
    const {appElement, proxy} = await createContextualTasksAppElement(
        /*url=*/ fixtureUrl,
        /*setupProxy=*/ undefined,
        /*waitForInitialLoadStart=*/ false);

    assertFalse(appElement.hasAttribute('is-ai-page_'));

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

    const {appElement, proxy} = await createContextualTasksAppElement(
        /*url=*/ initialThreadUrl.href,
        /*setupProxy=*/ (p) => {
          p.handler.setIsShownInTab(true);
        });

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

  test('logs back button metric in full tab', async () => {
    const threadUuid = 'ab12';
    const url = new URL(window.location.href);
    url.searchParams.set('chrome_task_id', threadUuid);
    window.history.pushState({}, '', url.href);

    const {appElement} = await createContextualTasksAppElement(
        /*url=*/ url.href,
        /*setupProxy=*/ (p) => {
          p.handler.setIsShownInTab(true);
        });
    const {promise, resolve} = Promise.withResolvers<void>();
    appElement.setPopStateFinishedCallbackForTesting(resolve);
    await microtasksFinished();

    window.dispatchEvent(new CustomEvent('popstate'));
    await promise;

    // Both recordUserAction and recordBoolean map to the same metric name in
    // the fake metrics tracker.
    assertEquals(
        2,
        metrics.count(
            'ContextualTasks.HistoryNavigation.UserAction.NavigatedInFullTab'));
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.HistoryNavigation.UserAction.NavigatedInFullTab',
            true));
  });

  test('sends composebox height update', async () => {
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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
    appElement.setForcedComposeboxBoundsForTesting(rect);
    await microtasksFinished();

    const frameRect = appElement.$.threadFrame.getBoundingClientRect();

    // Verify styles applied
    assertEquals('absolute', composebox.style.position);
    assertEquals(
        `${window.innerHeight - (frameRect.top + rect.bottom)}px`,
        composebox.style.bottom);
    assertEquals(`${frameRect.left + rect.left}px`, composebox.style.left);
    assertEquals(`${rect.width}px`, composebox.style.width);
    assertEquals('', composebox.style.height);

    // Verify zero state clears styles
    appElement.setIsZeroStateForTesting(true);
    await microtasksFinished();

    assertEquals('', composebox.style.position);
    assertEquals('', composebox.style.top);
    assertEquals('', composebox.style.left);
    assertEquals('', composebox.style.width);
    assertEquals('', composebox.style.height);
  });

  test('composebox bounds update styles in nlm', async () => {
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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
    appElement.setForcedComposeboxBoundsForTesting(rect);
    await microtasksFinished();
    await appElement.updateComplete;

    const frameRect = appElement.$.threadFrame.getBoundingClientRect();

    // Verify styles applied
    assertEquals('absolute', composebox.style.position);

    // Verify zero state clears styles
    appElement.setIsZeroStateForTesting(true);
    await microtasksFinished();
    await appElement.updateComplete;

    assertEquals('', composebox.style.position);

    // Verify inNlm restores styles even in zero state
    appElement.setInNlmForTesting(true);
    await microtasksFinished();
    await appElement.updateComplete;

    // Re-apply forced bounds as they were cleared in zero state.
    appElement.setForcedComposeboxBoundsForTesting(rect);
    await appElement.updateComplete;

    assertEquals('fixed', composebox.style.position);
    assertEquals(
        `${window.innerHeight - (frameRect.top + rect.bottom)}px`,
        composebox.style.bottom);
    assertEquals(`${frameRect.left + rect.left}px`, composebox.style.left);
    assertEquals(`${rect.width}px`, composebox.style.width);
    assertEquals('', composebox.style.height);
  });

  test('composebox hidden in nlm when no forced bounds', async () => {
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    appElement.setIsZeroStateForTesting(false);
    appElement.setInNlmForTesting(true);
    appElement.setIsZeroStateForTesting(false);
    await appElement.updateComplete;
    await microtasksFinished();

    assertTrue(appElement.$.composebox.hidden);

    // Set forced bounds
    const rect = {
      top: 10,
      left: 20,
      width: 100,
      height: 200,
      right: 120,
      bottom: 210,
    };
    appElement.setForcedComposeboxBoundsForTesting(rect);
    await appElement.updateComplete;
    await microtasksFinished();

    assertFalse(appElement.$.composebox.hidden);
  });

  test('composebox hidden when jump fix conditions met', async () => {
    loadTimeData.overrideValues({enableComposeboxJumpFix: true});
    const {appElement, proxy} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);
    await removeThreadFrameToPreventRaceConditions();

    const composebox = appElement.$.composebox;
    assertTrue(!!composebox);

    // Initial state setup: AI page, not zero state, no forced bounds
    proxy.handler.setIsAiPage(true);
    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    appElement.setIsZeroStateForTesting(false);
    appElement.setForcedComposeboxBoundsForTesting(null);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await appElement.updateComplete;

    assertTrue(composebox.hasAttribute('hidden'));

    // Set forced bounds, composebox should not be hidden
    appElement.setForcedComposeboxBoundsForTesting({
      top: 10, left: 20, width: 100, height: 200, right: 120, bottom: 210,
    });
    await microtasksFinished();
    await appElement.updateComplete;
    assertFalse(composebox.hasAttribute('hidden'));

    // Unset forced bounds, composebox should be hidden again
    appElement.setForcedComposeboxBoundsForTesting(null);
    await microtasksFinished();
    await appElement.updateComplete;
    assertTrue(composebox.hasAttribute('hidden'));

    // Set zero state, composebox should not be hidden
    appElement.setIsZeroStateForTesting(true);
    await microtasksFinished();
    await appElement.updateComplete;
    assertFalse(composebox.hasAttribute('hidden'));

    // Reset zero state, composebox should be hidden
    appElement.setIsZeroStateForTesting(false);
    await microtasksFinished();
    await appElement.updateComplete;
    assertTrue(composebox.hasAttribute('hidden'));

    // Set not AI page, composebox should not be hidden
    proxy.callbackRouterRemote.onAiPageStatusChanged(false);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await appElement.updateComplete;
    assertFalse(composebox.hasAttribute('hidden'));
  });

  test('updates clip path on post message', async () => {
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    // Get the webview element.
    const webview = appElement.shadowRoot.querySelector<HTMLElement>('webview');
    assertTrue(!!webview);

    // Wait for the load handler to finish to avoid a race condition
    // between the post message setting the forcedComposeboxBounds_ and the
    // load commit handler resetting it.

    // Simulate loadcommit to set up the target origin in PostMessageHandler.
    const loadCommitEvent = new Event('loadcommit');
    Object.assign(loadCommitEvent, {isTopLevel: true, url: fixtureUrl});
    webview.dispatchEvent(loadCommitEvent);
    const composebox = appElement.shadowRoot.querySelector<HTMLElement>(
        'contextual-tasks-composebox');
    assertTrue(!!composebox);
    // Simulate an input plate bounds update message.
    // Grab the current dimensions before sending the message
    const currentWidth = composebox.offsetWidth;
    const currentHeight = composebox.offsetHeight;
    const rect = {
      top: 0,
      left: 0,
      width: currentWidth,
      height: currentHeight,
      right: currentWidth,
      bottom: currentHeight,
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

    // Verify logic instead of hardcoded values to avoid brittle tests caused by
    // rendering height variations.
    const finalBounds = appElement.getForcedComposeboxBoundsForTesting();
    const actualDomHeight = appElement.$.composebox.offsetHeight;

    // Verify Calculated Properties.
    assertEquals(
        actualDomHeight, finalBounds!.height,
        'The stored height must match the actual DOM offsetHeight at runtime.');
    assertEquals(
        rect.bottom - actualDomHeight, finalBounds!.top,
        'The top coordinate must be correctly calculated as (bottom - actualHeight).');

    // Verify Pass-through Properties.
    assertEquals(
        rect.left, finalBounds!.left,
        'Left should be passed through unchanged.');
    assertEquals(
        rect.width, finalBounds!.width,
        'Width should be passed through unchanged.');
    assertEquals(
        rect.right, finalBounds!.right,
        'Right should be passed through unchanged.');
    assertEquals(
        rect.bottom, finalBounds!.bottom,
        'Bottom should be passed through unchanged.');
    assertDeepEquals([occluder], appElement.getOccludersForTesting());

    // Verify clip-path on webview
    const clipPath = webview.style.clipPath;
    assertTrue(
        clipPath.includes('path'), 'clip-path should contain path');
  });

  test('sets isFrameLoading to false when content load finishes', async () => {
    const {appElement} = await createContextualTasksAppElement(
        /*url=*/ fixtureUrl,
        /*setupProxy=*/ undefined,
        /*waitForInitialLoadStart=*/ false);

    // Remove the thread frame to prevent unwanted loadstart events.
    const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
    assertTrue(!!threadFrame);
    appElement.shadowRoot.removeChild(threadFrame);
    await microtasksFinished();

    const event = new Event('loadstart');
    Object.assign(event, {url: 'http://example.com', isTopLevel: true});
    appElement.onThreadFrameLoadStartForTesting(
        event as chrome.webviewTag.LoadStartEvent);

    const loadCommitEvent = new Event('loadcommit');
    Object.assign(
        loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
    appElement.onThreadFrameLoadCommitForTesting(
        loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
    await microtasksFinished();

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
    const {appElement} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

    // Remove the thread frame to prevent unwanted loadstart events.
    const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
    assertTrue(!!threadFrame);
    appElement.shadowRoot.removeChild(threadFrame);
    await microtasksFinished();

    const event = new Event('loadstart');
    Object.assign(event, {url: 'http://example.com', isTopLevel: true});
    appElement.onThreadFrameLoadStartForTesting(
        event as chrome.webviewTag.LoadStartEvent);

    // Simulate load abort.
    const loadAbortEvent = new CustomEvent('loadabort') as any;
    loadAbortEvent.isTopLevel = true;
    loadAbortEvent.url = fixtureUrl;
    loadAbortEvent.reason = 'ERR_CONNECTION_RESET';
    appElement.onThreadFrameLoadAbortForTesting(loadAbortEvent);
    await microtasksFinished();

    // Verify isFrameLoading is false.
    assertFalse(
        appElement.getIsFrameLoadingForTesting(),
        'isFrameLoading should be false');
  });

  test(
      'hides composebox if load abort contains an error document', async () => {
        const {appElement} = await createContextualTasksAppElement(
            /*url=*/ fixtureUrl,
            /*setupProxy=*/ (p) => {
              p.handler.isEmbeddedPageErrorDocument = () => {
                return Promise.resolve({isErrorDocument: true});
              };
            });

        // Remove the thread frame to prevent unwanted loadstart events.
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        assertTrue(!!threadFrame);
        appElement.shadowRoot.removeChild(threadFrame);
        await microtasksFinished();

        const loadAbortEvent = new CustomEvent('loadabort') as any;
        loadAbortEvent.isTopLevel = true;
        loadAbortEvent.url = fixtureUrl;
        loadAbortEvent.reason = 'ERR_CONNECTION_RESET';

        // Do NOT await yet.
        const promise =
            appElement.onThreadFrameLoadAbortForTesting(loadAbortEvent);

        await promise;

        // After it resolves it should be true.
        assertTrue(
            appElement.isLoadErrorForTesting,
            'isLoadError_ should be true if it was an error document');
      });

  test(
      'does not reset forced composebox bounds if navigation aborts',
      async () => {
        const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        await removeThreadFrameToPreventRaceConditions();

        // Set some initial forced bounds.
        const initialBounds = {
          top: 10, left: 20, width: 100, height: 200, right: 120, bottom: 210,
        };
        appElement.setForcedComposeboxBoundsForTesting(initialBounds);

        // Wait for any composebox height updates to process.
        await appElement.updateComplete;
        await microtasksFinished();
        const boundsBeforeNav = appElement.getForcedComposeboxBoundsForTesting();

        // Simulate navigation start.
        const loadStartEvent = new Event('loadstart');
        Object.assign(
            loadStartEvent, {url: 'http://example.com', isTopLevel: true});
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);
        await microtasksFinished();

        // Bounds should not be reset yet.
        assertDeepEquals(boundsBeforeNav, appElement.getForcedComposeboxBoundsForTesting());

        // Simulate load abort.
        const loadAbortEvent = new CustomEvent('loadabort') as any;
        loadAbortEvent.isTopLevel = true;
        loadAbortEvent.url = 'http://example.com';
        loadAbortEvent.reason = 'ERR_CONNECTION_RESET';

        const promise = appElement.onThreadFrameLoadAbortForTesting(loadAbortEvent);
        await promise;

        // Bounds should still be present.
        assertDeepEquals(boundsBeforeNav, appElement.getForcedComposeboxBoundsForTesting()!);
      });

  test(
      'does not hide composebox if load abort does not contain error document',
      async () => {
        const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
        // Override isEmbeddedPageErrorDocument to return false
        proxy.handler.isEmbeddedPageErrorDocument = () => {
          return Promise.resolve({isErrorDocument: false});
        };
        BrowserProxyImpl.setInstance(proxy);

        const appElement = document.createElement('contextual-tasks-app');
        document.body.appendChild(appElement);
        await microtasksFinished();

        // Remove the thread frame to prevent unwanted loadstart events.
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        assertTrue(!!threadFrame);
        appElement.shadowRoot.removeChild(threadFrame);
        await microtasksFinished();

        const loadAbortEvent = new CustomEvent('loadabort') as any;
        loadAbortEvent.isTopLevel = true;
        loadAbortEvent.url = fixtureUrl;
        loadAbortEvent.reason = 'ERR_ABORTED';

        // Do NOT await yet.
        const promise =
            appElement.onThreadFrameLoadAbortForTesting(loadAbortEvent);

        await promise;

        // After it resolves it should be false.
        assertFalse(
            appElement.isLoadErrorForTesting,
            'isLoadError_ should be false if it was not an error document');
      });

  test('addCommonSearchParams overrides parameters except cs', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));
    await microtasksFinished();

    const appElement: any = document.querySelector('contextual-tasks-app');

    appElement.commonSearchParams_ = {
      'cs': '1',
      'hl': 'en',
      'gsc': '2',
    };

    let url = new URL('https://example.com');
    url = appElement.addCommonSearchParams(url);
    assertEquals('1', url.searchParams.get('cs'));
    assertEquals('en', url.searchParams.get('hl'));
    assertEquals('2', url.searchParams.get('gsc'));

    url = new URL('https://example.com?hl=override_hl&gsc=override_gsc');
    url = appElement.addCommonSearchParams(url);
    assertEquals('1', url.searchParams.get('cs'));
    assertEquals('override_hl', url.searchParams.get('hl'));
    assertEquals('override_gsc', url.searchParams.get('gsc'));

    url = new URL('https://example.com?cs=0&hl=another');
    url = appElement.addCommonSearchParams(url);
    assertEquals('1', url.searchParams.get('cs'));
    assertEquals('another', url.searchParams.get('hl'));
  });
});
