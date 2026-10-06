// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/toolbar_app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ToolbarBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_toolbar_browser_proxy.js';
import type {ContextualTasksToolbarAppElement} from 'chrome://contextual-tasks/toolbar_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestContextualTasksBrowserProxy, TestToolbarBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('ToolbarAppTest', () => {
  let toolbarApp: ContextualTasksToolbarAppElement;
  let proxy: TestContextualTasksBrowserProxy;
  let toolbarProxy: TestToolbarBrowserProxy;

  setup(async () => {
    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);
    toolbarProxy = new TestToolbarBrowserProxy();
    ToolbarBrowserProxyImpl.setInstance(toolbarProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbarApp = document.createElement('contextual-tasks-toolbar-app');
    document.body.appendChild(toolbarApp);
    await toolbarApp.updateComplete;
  });

  test('element is instantiated correctly with top-toolbar', () => {
    assertEquals('CONTEXTUAL-TASKS-TOOLBAR-APP', toolbarApp.tagName);
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar');
    assertTrue(!!topToolbar);
    assertEquals('toolbar', topToolbar.id);
  });

  test(
      'thread title propagates to top-toolbar title and document title',
      async () => {
        const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
        const titleDiv =
            topToolbar.shadowRoot.querySelector('.top-toolbar-title')!;

        // Initial title is empty.
        assertEquals('', titleDiv.textContent.trim());

        // Update via Mojo.
        toolbarProxy.callbackRouterRemote.setThreadTitle(
            'My Active Task Thread');
        await toolbarProxy.callbackRouterRemote.$.flushForTesting();
        await toolbarApp.updateComplete;
        await topToolbar.updateComplete;

        // Verify title propagates to DOM and document.title.
        assertEquals('My Active Task Thread', titleDiv.textContent.trim());
        assertEquals('My Active Task Thread', document.title);

        // Clearing title via Mojo resets UI title and restores default document
        // title.
        toolbarProxy.callbackRouterRemote.setThreadTitle('');
        await toolbarProxy.callbackRouterRemote.$.flushForTesting();
        await toolbarApp.updateComplete;
        await topToolbar.updateComplete;

        assertEquals('', titleDiv.textContent.trim());
        assertEquals(loadTimeData.getString('title'), document.title);
      });

  test('thread title listener is removed on disconnect', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
    const titleDiv = topToolbar.shadowRoot.querySelector('.top-toolbar-title')!;

    // Disconnect toolbarApp from DOM.
    toolbarApp.remove();

    // Sending title update via Mojo should not affect the disconnected
    // component.
    toolbarProxy.callbackRouterRemote.setThreadTitle('Disconnected Thread');
    await toolbarProxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;

    assertEquals('', titleDiv.textContent.trim());
  });

  test('ai page status propagates is-ai-page attribute', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;

    // Initial state.
    assertFalse(topToolbar.hasAttribute('is-ai-page'));

    // Update via Mojo.
    toolbarProxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await toolbarProxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;
    await topToolbar.updateComplete;

    // Verify state reflects to attribute.
    assertTrue(topToolbar.hasAttribute('is-ai-page'));
  });

  test('side panel state change updates theme from url', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
    // Simulate url with dark mode param.
    const url = new URL(window.location.href);
    url.searchParams.set('cs', '1');
    window.history.replaceState({}, '', url.href);

    toolbarProxy.callbackRouterRemote.onSidePanelStateChanged();
    await toolbarProxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;
    await topToolbar.updateComplete;

    assertTrue(topToolbar.hasAttribute('dark-mode'));

    // Switch to light mode.
    url.searchParams.set('cs', '0');
    window.history.replaceState({}, '', url.href);

    toolbarProxy.callbackRouterRemote.onSidePanelStateChanged();
    await toolbarProxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;
    await topToolbar.updateComplete;

    assertFalse(topToolbar.hasAttribute('dark-mode'));
  });

  test(
      'side panel state change listener is removed on disconnect', async () => {
        const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
        toolbarApp.remove();

        const url = new URL(window.location.href);
        url.searchParams.set('cs', '1');
        window.history.replaceState({}, '', url.href);

        toolbarProxy.callbackRouterRemote.onSidePanelStateChanged();
        await toolbarProxy.callbackRouterRemote.$.flushForTesting();
        await toolbarApp.updateComplete;
        await topToolbar.updateComplete;

        assertFalse(topToolbar.hasAttribute('dark-mode'));
      });

  test('new thread click invokes browser proxy handler', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;

    // Setup eligibility so newThreadButton is not hidden.
    topToolbar.isAimEligible = true;
    await topToolbar.updateComplete;

    const newThreadButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#newThreadButton')!;
    assertTrue(!!newThreadButton);

    newThreadButton.click();
    await toolbarProxy.handler.whenCalled('createNewThread');
  });
});
