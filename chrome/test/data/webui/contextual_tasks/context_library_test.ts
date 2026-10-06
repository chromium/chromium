// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/contextual_tasks_extension/context_library.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ContextLibraryElement} from 'chrome://contextual-tasks/contextual_tasks_extension/context_library.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

function createTab(tabId: number, url: string): TabInfo {
  return {
    tabId,
    title: `Tab ${tabId}`,
    url,
    showInCurrentTabChip: false,
    showInPreviousTabChip: false,
    isLoading: false,
    lastActive: {internalValue: 0n},
  };
}

suite('ContextLibraryTest', () => {
  let app: ContextLibraryElement;
  let browserProxy: TestExtensionBrowserProxy;

  // Resolves the next time the element asks the embedder to re-measure it.
  let nextRequestResize: Promise<void>;
  let resolveRequestResize: () => void;

  function iframeRequestResize() {
    nextRequestResize = new Promise<void>(resolve => {
      resolveRequestResize = resolve;
    });
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(browserProxy);

    iframeRequestResize();
    window.requestResize = () => {
      resolveRequestResize();
    };

    app = document.createElement('context-library');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  teardown(() => {
    delete window.requestResize;
  });

  test(
      'Initializes with empty tabs if no restored tabs, ' +
          'and renders child group',
      () => {
        assertTrue(!!app);
        const faviconGroup = app.$.faviconGroup;
        assertTrue(!!faviconGroup);
        assertEquals(0, app.tabs.length);

        const items = faviconGroup.shadowRoot.querySelectorAll('.favicon-item');
        assertEquals(0, items.length);

        const moreItems = faviconGroup.shadowRoot.querySelector('#more-items');
        assertEquals(null, moreItems);
      });

  test('Renders favicon items when tabs are provided', async () => {
    const faviconGroup = app.$.faviconGroup;

    app.tabs = [
      createTab(1, 'https://www.google.com'),
      createTab(2, 'https://www.youtube.com'),
    ];
    await microtasksFinished();

    const items = faviconGroup.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(2, items.length);

    const moreItems = faviconGroup.shadowRoot.querySelector('#more-items');
    assertEquals(null, moreItems);
  });

  test(
      'Renders maximum 3 tabs with overflow badge for remaining tabs',
      async () => {
        const faviconGroup = app.$.faviconGroup;

        app.tabs = [
          createTab(1, 'https://www.google.com'),
          createTab(2, 'https://www.youtube.com'),
          createTab(3, 'https://en.wikipedia.org'),
          createTab(4, 'https://github.com'),
          createTab(5, 'https://www.chromium.org'),
        ];
        await microtasksFinished();

        const items = faviconGroup.shadowRoot.querySelectorAll('.favicon-item');
        assertEquals(3, items.length);

        const moreItems = faviconGroup.shadowRoot.querySelector('#more-items');
        assertTrue(!!moreItems);
        assertEquals('+2', moreItems.textContent.trim());
      });

  test('Triggers window.requestResize when element resizes', async () => {
    // Adding tabs grows the element, which ResizeObserver reports and the
    // element forwards to the embedder. Awaiting the call itself avoids any
    // dependency on a fixed delay.
    iframeRequestResize();
    app.tabs = [
      createTab(1, 'https://www.google.com'),
      createTab(2, 'https://www.youtube.com'),
    ];

    await nextRequestResize;
  });

  test('Forwards submittedTabIds to child favicon group', async () => {
    const submitted = new Set<number>([1, 3]);
    app.submittedTabIds = submitted;
    app.tabs = [
      createTab(1, 'https://www.google.com'),
      createTab(2, 'https://www.youtube.com'),
    ];
    await microtasksFinished();

    assertEquals(submitted, app.$.faviconGroup.submittedTabIds);
  });

  test('Updates tabs via SET_TABS postMessage', async () => {
    const messageDelivered = eventToPromise('message', window);
    window.postMessage(
        {
          type: 'SET_TABS',
          tabs: [
            createTab(1, 'https://www.google.com'),
            createTab(2, 'https://www.youtube.com'),
          ],
        },
        '*');
    await messageDelivered;
    await microtasksFinished();

    assertEquals(2, app.tabs.length);
    const items =
        app.$.faviconGroup.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(2, items.length);
  });

  test(
      'Updates tabs and submittedTabIds when onTabContextUpdated is dispatched',
      async () => {
        const tabs = [
          createTab(10, 'https://www.google.com'),
          createTab(20, 'https://www.youtube.com'),
        ];
        browserProxy.callbackRouterRemote.onTabContextUpdated(tabs, [20]);
        await browserProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertEquals(2, app.tabs.length);
        assertEquals(10, app.tabs[0]!.tabId);
        assertEquals(20, app.tabs[1]!.tabId);
        assertTrue(app.submittedTabIds.has(20));
        assertFalse(app.submittedTabIds.has(10));
        assertTrue(app.$.faviconGroup.submittedTabIds.has(20));

        const items =
            app.$.faviconGroup.shadowRoot.querySelectorAll('.favicon-item');
        assertEquals(2, items.length);
      });

  test('Stops updating after element is disconnected', async () => {
    app.remove();

    browserProxy.callbackRouterRemote.onTabContextUpdated(
        [createTab(10, 'https://www.google.com')], [10]);
    await browserProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(0, app.tabs.length);
    assertEquals(0, app.submittedTabIds.size);
  });
});
