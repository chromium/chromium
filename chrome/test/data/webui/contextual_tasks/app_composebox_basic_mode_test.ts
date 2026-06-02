// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

suite('ContextualTasksAppComposeboxBasicModeTest', function() {
  let initialUrl: string;

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
      composeboxSmartTabSharingVisible: false,
    });
    const proxy = new TestContextualTasksBrowserProxy('http://example.com');
    BrowserProxyImpl.setInstance(proxy);
  });

  test(
      'composebox z-index changes when visibility toggles with enableBasicModeZOrder',
      async () => {
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        const composebox =
            appElement.shadowRoot.querySelector('contextual-tasks-composebox');
        const threadFrame = appElement.shadowRoot.querySelector('#threadFrame');
        const flexCenterContainer =
            appElement.shadowRoot.querySelector('#flexCenterContainer');

        assertTrue(!!composebox);
        assertFalse(composebox.hasAttribute('hidden'));

        // Enter basic mode.
        proxy.callbackRouterRemote.enterBasicMode();
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

        // Restore the compose box (exit basic mode).
        proxy.callbackRouterRemote.exitBasicMode();
        await proxy.callbackRouterRemote.$.flushForTesting();
        assertFalse(composebox.hasAttribute('hidden'));

        const threadFrameStyleRestored = getComputedStyle(threadFrame!);
        const flexCenterStyleRestored = getComputedStyle(flexCenterContainer!);

        // Verify z-index is not stuck
        assertFalse(threadFrameStyleRestored.zIndex === '1');
        assertFalse(flexCenterStyleRestored.zIndex === '0');
      });

  // TODO(merced): Flakey on Linux DBG, so disabled while I debug.
  test.skip(
      'composebox visibility toggles with enableBasicModeZOrder set to false',
      async () => {
        loadTimeData.overrideValues({enableBasicModeZOrder: false});
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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

  test(
      'does not force enter basic mode when thread history is open if flag is disabled',
      async () => {
        loadTimeData.overrideValues(
            {forceBasicModeIfOpeningThreadHistory: false});
        const fixtureUrlWithHistory = new URL(fixtureUrl);
        fixtureUrlWithHistory.searchParams.set('atvm', '1');

        const {appElement} = await createContextualTasksAppElement(
            /*url=*/ fixtureUrlWithHistory.toString(),
            /*setupProxy=*/ (p) => {
              p.handler.setIsShownInTab(true);
            });

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
        const {appElement} = await createContextualTasksAppElement(
            /*url=*/ fixtureUrlWithHistory.toString(), /*setupProxy=*/ (p) => {
              p.handler.setIsShownInTab(true);
            });

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

  test(
      'sets basic mode when navigating from AI page and backend sends notification',
      async () => {
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        await removeThreadFrameToPreventRaceConditions();

        // Verify initial state.
        assertFalse(
            appElement.hasAttribute('is-in-basic-mode_'),
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
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);

        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        const navigationFinished = new Promise<void>(resolve => {
          appElement.setOnLoadStartFinishedCallbackForTesting(resolve);
        });
        appElement.onThreadFrameLoadCommitForTesting(
            loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
        await navigationFinished;
        await appElement.updateComplete;

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        // Misc notifications do not cause z-index flickering:
        proxy.callbackRouterRemote.restoreInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        proxy.callbackRouterRemote.hideInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        proxy.callbackRouterRemote.restoreInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Simulate navigation complete. Basic mode should not be updated
        // based on the last submitted state request from the backend.
        appElement.onThreadFrameContentLoadForTesting();
        await microtasksFinished();
        assertFalse(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should change to basic mode false due to backend after navigation completes');
        assertFalse(
            appElement.isNavigatingForTesting(),
            'Should not be navigating after navigation completes');
      });

  test(
      'sets basic mode as true when navigating due to backend sending notification',
      async () => {
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        await removeThreadFrameToPreventRaceConditions();

        // Verify initial state.
        assertFalse(
            appElement.hasAttribute('is-in-basic-mode_'),
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
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);

        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        const navigationFinished = new Promise<void>(resolve => {
          appElement.setOnLoadStartFinishedCallbackForTesting(resolve);
        });
        appElement.onThreadFrameLoadCommitForTesting(
            loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
        await navigationFinished;
        await appElement.updateComplete;

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        // Misc notifications do not cause z-index flickering:
        proxy.callbackRouterRemote.hideInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        proxy.callbackRouterRemote.restoreInput();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should be in basic mode when navigating from an AI page');
        assertTrue(
            appElement.isNavigatingForTesting(),
            'Should be navigating after navigation starts');

        proxy.callbackRouterRemote.enterBasicMode();
        await proxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Simulate navigation complete. Basic mode should not be updated
        // based on the last submitted state request from the backend.
        // Basic mode is true based on usage of `hideInput`.
        appElement.onThreadFrameContentLoadForTesting();
        await microtasksFinished();
        assertTrue(
            appElement.hasAttribute('is-in-basic-mode_'),
            'Should change to basic mode true due to backend after navigation completes');
        assertFalse(
            appElement.isNavigatingForTesting(),
            'Should not be navigating after navigation completes');
      });

  test(
      'does not set basic mode when navigating from AI page to non-AI page',
      async () => {
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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
    const {appElement, proxy} =
        await createContextualTasksAppElement(/*url=*/ fixtureUrl);

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

        const {appElement, proxy} = await createContextualTasksAppElement(
            /*url=*/ fixtureUrlWithHistory.toString(),
            /*setupProxy=*/ (p) => {
              p.handler.setIsShownInTab(true);
            });

        await removeThreadFrameToPreventRaceConditions();

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
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);

        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        const navigationFinished = new Promise<void>(resolve => {
          appElement.setOnLoadStartFinishedCallbackForTesting(resolve);
        });
        appElement.onThreadFrameLoadCommitForTesting(
            loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
        await navigationFinished;
        await appElement.updateComplete;

        // Should still be in basic mode during navigation.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
        assertTrue(appElement.isNavigatingForTesting());

        // Simulate navigation complete.
        appElement.onThreadFrameContentLoadForTesting();
        await microtasksFinished();

        // Should still be in basic mode after navigation (restored).
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));
        assertFalse(appElement.isNavigatingForTesting());
      });

  test(
      'sets pending basic mode to false when navigating from AI page and initially not in basic mode',
      async () => {
        loadTimeData.overrideValues({enableBasicMode: true});
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        await removeThreadFrameToPreventRaceConditions();
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
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);

        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        const navigationFinished = new Promise<void>(resolve => {
          appElement.setOnLoadStartFinishedCallbackForTesting(resolve);
        });
        appElement.onThreadFrameLoadCommitForTesting(
            loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
        await navigationFinished;
        await appElement.updateComplete;

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Verify pendingBasicMode_ is false (private property access).
        assertFalse(appElement.getPendingBasicModeForTesting()!);

        // Simulate navigation complete.
        appElement.onThreadFrameContentLoadForTesting();
        await microtasksFinished();

        // Should exit basic mode because pendingBasicMode_ was false.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
      });

  test(
      'does not set pending basic mode when navigating from AI page and initially in basic mode',
      async () => {
        loadTimeData.overrideValues({enableBasicMode: true});
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        // Force into basic mode initially.
        proxy.callbackRouterRemote.enterBasicMode();
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
        assertEquals(null, appElement.getPendingBasicModeForTesting());

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
        const {appElement, proxy} =
            await createContextualTasksAppElement(/*url=*/ fixtureUrl);

        await removeThreadFrameToPreventRaceConditions();

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
        appElement.onThreadFrameLoadStartForTesting(
            loadStartEvent as chrome.webviewTag.LoadStartEvent);

        const loadCommitEvent = new Event('loadcommit');
        Object.assign(
            loadCommitEvent, {url: 'http://example.com', isTopLevel: true});
        const navigationFinished = new Promise<void>(resolve => {
          appElement.setOnLoadStartFinishedCallbackForTesting(resolve);
        });
        appElement.onThreadFrameLoadCommitForTesting(
            loadCommitEvent as chrome.webviewTag.LoadCommitEvent);
        await navigationFinished;
        await appElement.updateComplete;

        // Should be in basic mode now because the app is navigating from an AI
        // page.
        assertTrue(appElement.hasAttribute('is-in-basic-mode_'));

        // Verify pendingBasicMode_ is false (private property access).
        assertFalse(appElement.getPendingBasicModeForTesting()!);

        // Simulate load commit.
        appElement.onThreadFrameContentLoadForTesting();
        await microtasksFinished();

        // Should exit basic mode because pendingBasicMode_ was false.
        assertFalse(appElement.hasAttribute('is-in-basic-mode_'));
      });

  test('enters NLM mode if initial URL has NLM param', async () => {
    loadTimeData.overrideValues({
      enableCustomNlmUi: true,
      nlmUrlParam: 'ajid',
    });

    const nlmFixtureUrl = new URL(fixtureUrl);
    nlmFixtureUrl.searchParams.set('ajid', '1');

    const {appElement} = await createContextualTasksAppElement(
        /*url=*/ nlmFixtureUrl.toString());

    // Verify composeboxHeaderWrapper is hidden.
    const headerWrapper =
        appElement.shadowRoot.querySelector('#composeboxHeaderWrapper');
    assertTrue(!!headerWrapper);
    assertTrue(headerWrapper.hasAttribute('hidden'));
  });


});
