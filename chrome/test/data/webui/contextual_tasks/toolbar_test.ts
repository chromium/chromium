// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/toolbar_app.js';

import {ToolbarBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_toolbar_browser_proxy.js';
import type {ContextualTasksToolbarAppElement} from 'chrome://contextual-tasks/toolbar_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {setupTestHandler} from './contextual_tasks_test_utils.js';
import {TestToolbarBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('ToolbarAppTest', () => {
  let toolbarApp: ContextualTasksToolbarAppElement;
  let toolbarProxy: TestToolbarBrowserProxy;

  setup(async () => {
    setupTestHandler('chrome://webui-test/contextual_tasks/test.html');
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

  test('new thread click invokes browser proxy handler', async () => {
    const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;

    // Setup eligibility so newThreadButton is not hidden.
    topToolbar.isAimEligible = true;
    await topToolbar.updateComplete;

    const newThreadButton = topToolbar.$.newThreadButton;

    newThreadButton.click();
    await toolbarProxy.handler.whenCalled('createNewThread');
  });

  test(
      'suppresses mouse and keyboard contextmenu events when enabled',
      async () => {
        const originalSuppressContextMenu =
            loadTimeData.getBoolean('suppressContextMenu');
        try {
          loadTimeData.overrideValues({suppressContextMenu: true});
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          toolbarApp = document.createElement('contextual-tasks-toolbar-app');
          document.body.appendChild(toolbarApp);
          await toolbarApp.updateComplete;

          const topToolbar =
              toolbarApp.shadowRoot.querySelector('top-toolbar')!;

          // Right-click (button 2) bubbling across shadow boundaries.
          const mouseContextMenuEvent = new MouseEvent('contextmenu', {
            bubbles: true,
            button: 2,
            cancelable: true,
            composed: true,
          });
          assertFalse(topToolbar.dispatchEvent(mouseContextMenuEvent));
          assertTrue(mouseContextMenuEvent.defaultPrevented);

          // Keyboard ContextMenu / Shift+F10 (button 0).
          const keyboardContextMenuEvent = new MouseEvent('contextmenu', {
            bubbles: true,
            button: 0,
            cancelable: true,
            composed: true,
          });
          assertFalse(topToolbar.dispatchEvent(keyboardContextMenuEvent));
          assertTrue(keyboardContextMenuEvent.defaultPrevented);

          toolbarApp.remove();
          const disconnectedEvent = new MouseEvent('contextmenu', {
            bubbles: true,
            button: 0,
            cancelable: true,
            composed: true,
          });
          assertTrue(toolbarApp.dispatchEvent(disconnectedEvent));
          assertFalse(disconnectedEvent.defaultPrevented);
        } finally {
          loadTimeData.overrideValues(
              {suppressContextMenu: originalSuppressContextMenu});
        }
      });

  test('allows contextmenu events when suppression is disabled', async () => {
    const originalSuppressContextMenu =
        loadTimeData.getBoolean('suppressContextMenu');
    try {
      loadTimeData.overrideValues({suppressContextMenu: false});
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      toolbarApp = document.createElement('contextual-tasks-toolbar-app');
      document.body.appendChild(toolbarApp);
      await toolbarApp.updateComplete;

      const topToolbar = toolbarApp.shadowRoot.querySelector('top-toolbar')!;
      const mouseContextMenuEvent = new MouseEvent('contextmenu', {
        bubbles: true,
        button: 2,
        cancelable: true,
        composed: true,
      });
      assertTrue(topToolbar.dispatchEvent(mouseContextMenuEvent));
      assertFalse(mouseContextMenuEvent.defaultPrevented);
    } finally {
      loadTimeData.overrideValues(
          {suppressContextMenu: originalSuppressContextMenu});
    }
  });
});
