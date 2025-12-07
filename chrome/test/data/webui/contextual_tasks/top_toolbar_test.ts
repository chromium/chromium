// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('TopToolbarTest', () => {
  let topToolbar: TopToolbarElement;
  let proxy: TestContextualTasksBrowserProxy;

  function assertHTMLElement(element: Element|null|undefined):
      asserts element is HTMLElement {
    assertTrue(!!element);
    assertTrue(element instanceof HTMLElement);
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);

    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
  });

  test('shows correct logo', () => {
    assertEquals(
        topToolbar.$.topToolbarLogo.src,
        // <if expr="_google_chrome">
        'chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg',
        // </if>
        // <if expr="not _google_chrome">
        'chrome://resources/cr_components/searchbox/icons/chrome_product.svg',
        // </if>
    );
  });

  test('handles new thread button click', async () => {
    const newThreadButton = topToolbar.shadowRoot.querySelector(
        'cr-icon-button[title="New Thread"]');
    assertHTMLElement(newThreadButton);
    const newThreadEvent = eventToPromise('new-thread-click', topToolbar);
    newThreadButton.click();
    await newThreadEvent;
  });

  test('handles thread history button click', async () => {
    const historyButton = topToolbar.shadowRoot.querySelector(
        'cr-icon-button[title="Thread History"]');
    assertHTMLElement(historyButton);
    const historyEvent = eventToPromise('thread-history-click', topToolbar);
    historyButton.click();
    await historyEvent;
  });

  test('handles close button click', async () => {
    const closeButton =
        topToolbar.shadowRoot.querySelector('cr-icon-button[title="Close"]');
    assertHTMLElement(closeButton);
    closeButton.click();
    await proxy.handler.whenCalled('closeSidePanel');
  });

  test('toggles sources button visibility', async () => {
    const sourcesButton = topToolbar.shadowRoot.querySelector('#sources');
    assertHTMLElement(sourcesButton);
    assertTrue(sourcesButton.hasAttribute('hidden'));

    topToolbar.attachedTabs =
        [{tabId: 1, title: 'Tab 1', url: {url: 'https://example.com'}}];
    await microtasksFinished();

    assertFalse(sourcesButton.hasAttribute('hidden'));
  });

  test('handles sources menu interactions', async () => {
    const tab = {tabId: 1, title: 'Tab 1', url: {url: 'https://example.com'}};
    topToolbar.attachedTabs = [tab];
    await microtasksFinished();

    const sourcesButton = topToolbar.shadowRoot.querySelector('#sources');
    assertHTMLElement(sourcesButton);
    sourcesButton.click();
    await microtasksFinished();

    assertTrue(topToolbar.$.sourcesMenu.get().open);

    const tabButton =
        topToolbar.$.sourcesMenu.get().querySelector('button.dropdown-item');
    assertHTMLElement(tabButton);
    tabButton.click();

    const [tabId, url] =
        await proxy.handler.whenCalled('onTabClickedFromSourcesMenu');
    assertEquals(tabId, 1);
    assertDeepEquals(url, tab.url);
  });

  test('handles more menu interactions', async () => {
    const moreButton = topToolbar.shadowRoot.querySelector('#more');
    assertHTMLElement(moreButton);
    moreButton.click();
    await microtasksFinished();

    assertTrue(topToolbar.$.menu.get().open);

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    assertEquals(3, buttons.length);
  });

  test('handles open in new tab click', async () => {
    const moreButton = topToolbar.shadowRoot.querySelector('#more');
    assertHTMLElement(moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const openInNewTabButton = buttons[0];
    assertHTMLElement(openInNewTabButton);
    openInNewTabButton.click();
    await proxy.handler.whenCalled('moveTaskUiToNewTab');
  });

  test('handles my activity click', async () => {
    const moreButton = topToolbar.shadowRoot.querySelector('#more');
    assertHTMLElement(moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const myActivityButton = buttons[1];
    assertHTMLElement(myActivityButton);
    myActivityButton.click();
    await proxy.handler.whenCalled('openMyActivityUi');
  });

  test('handles help click', async () => {
    const moreButton = topToolbar.shadowRoot.querySelector('#more');
    assertHTMLElement(moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const helpButton = buttons[2];
    assertHTMLElement(helpButton);
    helpButton.click();
    await proxy.handler.whenCalled('openHelpUi');
  });
});
