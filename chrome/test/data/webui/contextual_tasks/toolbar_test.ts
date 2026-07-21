// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/toolbar_app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ContextualTasksToolbarAppElement} from 'chrome://contextual-tasks/toolbar_app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('ToolbarAppTest', () => {
  let toolbarApp: ContextualTasksToolbarAppElement;
  let proxy: TestContextualTasksBrowserProxy;

  setup(async () => {
    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);

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

  test('thread title propagates to top-toolbar title', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
    const titleDiv = topToolbar.shadowRoot.querySelector('.top-toolbar-title')!;

    // Initial title is empty.
    assertEquals('', titleDiv.textContent.trim());

    // Update via Mojo.
    proxy.callbackRouterRemote.setThreadTitle('My Active Task Thread');
    await proxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;
    await topToolbar.updateComplete;

    // Verify title propagates to DOM.
    assertEquals('My Active Task Thread', titleDiv.textContent.trim());
  });

  test('ai page status propagates is-ai-page attribute', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;

    // Initial state.
    assertFalse(topToolbar.hasAttribute('is-ai-page'));

    // Update via Mojo.
    proxy.callbackRouterRemote.onAiPageStatusChanged(true);
    await proxy.callbackRouterRemote.$.flushForTesting();
    await toolbarApp.updateComplete;
    await topToolbar.updateComplete;

    // Verify state reflects to attribute.
    assertTrue(topToolbar.hasAttribute('is-ai-page'));
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
    await proxy.handler.whenCalled('createNewThread');
  });
});
