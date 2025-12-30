// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';
import 'chrome://contextual-tasks/sources_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ContextualTasksFaviconGroupElement} from 'chrome://contextual-tasks/favicon_group.js';
import type {SourcesMenuElement} from 'chrome://contextual-tasks/sources_menu.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertHTMLElement} from './test_utils.js';

suite('TopToolbarTest', () => {
  let topToolbar: TopToolbarElement;
  let proxy: TestContextualTasksBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);

    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
  });

  test('shows correct logo', () => {
    // <if expr="_google_chrome">
    const logo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
        '.top-toolbar-logo');
    assertHTMLElement(logo);
    assertEquals(
        logo.src,
        'chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg');
    // </if>
    // <if expr="not _google_chrome">
    const lightLogo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
        '.chrome-logo-light');
    assertHTMLElement(lightLogo);
    assertEquals(
        lightLogo.src,
        'chrome://resources/cr_components/searchbox/icons/chrome_product.svg');
    const darkLogo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
        '.chrome-logo-dark');
    assertHTMLElement(darkLogo);
    assertEquals(
        darkLogo.src, 'chrome://resources/images/chrome_logo_dark.svg');
    // </if>
  });

  test('handles new thread button click', async () => {
    const newThreadButton = topToolbar.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button[title="New Thread"]');
    assertTrue(!!newThreadButton);
    const newThreadEvent = eventToPromise('new-thread-click', topToolbar);
    newThreadButton.click();
    await newThreadEvent;
  });

  test('handles thread history button click', async () => {
    const historyButton = topToolbar.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button[title="Thread History"]');
    assertTrue(!!historyButton);
    historyButton.click();
    await proxy.handler.whenCalled('showThreadHistory');
  });

  test('handles close button click', async () => {
    const closeButton = topToolbar.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button[title="Close"]');
    assertTrue(!!closeButton);
    closeButton.click();
    await proxy.handler.whenCalled('closeSidePanel');
  });

  test('toggles sources button visibility', async () => {
    const sourcesButton =
        topToolbar.shadowRoot.querySelector<ContextualTasksFaviconGroupElement>(
            '#sources');
    assertTrue(!!sourcesButton);

    // Initially, there are no attached tabs, so the favicon group should not
    // render any items.
    assertFalse(!!sourcesButton.shadowRoot.querySelector('.favicon-item'));

    topToolbar.attachedTabs =
        [{tabId: 1, title: 'Tab 1', url: {url: 'https://example.com'}}];
    await microtasksFinished();

    // After attaching a tab, the favicon group should render a favicon item.
    assertTrue(!!sourcesButton.shadowRoot.querySelector('.favicon-item'));
  });

  test('handles sources menu interactions', async () => {
    const tab = {tabId: 1, title: 'Tab 1', url: {url: 'https://example.com'}};
    topToolbar.attachedTabs = [tab];
    await microtasksFinished();

    const sourcesButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
    assertTrue(!!sourcesButton);
    sourcesButton.click();
    await microtasksFinished();

    const sourcesMenuElement: SourcesMenuElement =
        topToolbar.$.sourcesMenu.get();
    const crActionMenu =
        sourcesMenuElement.shadowRoot.querySelector<CrActionMenuElement>(
            'cr-action-menu');
    assertTrue(!!crActionMenu);
    assertTrue(crActionMenu.open);

    // The first header is "Shared tabs and files", the second (optional) is
    // "Tabs". We expect only 1 header since we only have one type of item
    // (tabs) and the "Tabs" header should be hidden.
    const headers = sourcesMenuElement.shadowRoot.querySelectorAll('.header');
    assertEquals(1, headers.length);

    // Click the first tab item.
    const tabButton = sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
        'button.dropdown-item');
    assertTrue(!!tabButton);
    tabButton.click();

    const [tabId, url] =
        await proxy.handler.whenCalled('onTabClickedFromSourcesMenu');
    assertEquals(tabId, 1);
    assertDeepEquals(url, tab.url);
  });

  test('handles more menu interactions', async () => {
    const moreButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#more');
    assertTrue(!!moreButton);
    moreButton.click();
    await microtasksFinished();

    assertTrue(topToolbar.$.menu.get().open);

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    assertEquals(3, buttons.length);
  });

  test('handles open in new tab click', async () => {
    topToolbar.isAiPage = true;
    await microtasksFinished();

    const moreButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#more');
    assertTrue(!!moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const openInNewTabButton = buttons[0];
    assertTrue(!!openInNewTabButton);
    assertFalse(openInNewTabButton.disabled);
    openInNewTabButton.click();
    await proxy.handler.whenCalled('moveTaskUiToNewTab');

    topToolbar.isAiPage = false;
    await microtasksFinished();
    assertTrue(openInNewTabButton.disabled);
    proxy.handler.reset();
    openInNewTabButton.click();
    assertEquals(0, proxy.handler.getCallCount('moveTaskUiToNewTab'));
  });

  test('handles my activity click', async () => {
    const moreButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#more');
    assertTrue(!!moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const myActivityButton = buttons[1];
    assertTrue(!!myActivityButton);
    myActivityButton.click();
    await proxy.handler.whenCalled('openMyActivityUi');
  });

  test('handles help click', async () => {
    const moreButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#more');
    assertTrue(!!moreButton);
    moreButton.click();
    await microtasksFinished();

    const buttons = topToolbar.$.menu.get().querySelectorAll('button');
    const helpButton = buttons[2];
    assertTrue(!!helpButton);
    helpButton.click();
    await proxy.handler.whenCalled('openHelpUi');
  });

  test('shows 3 tab icons without number for 3 tabs', async () => {
    const sourcesButton =
        topToolbar.shadowRoot.querySelector<ContextualTasksFaviconGroupElement>(
            '#sources');
    assertTrue(!!sourcesButton);

    topToolbar.attachedTabs = [
      {tabId: 1, title: 'Tab 1', url: {url: 'https://example.com/1'}},
      {tabId: 2, title: 'Tab 2', url: {url: 'https://example.com/2'}},
      {tabId: 3, title: 'Tab 3', url: {url: 'https://example.com/3'}},
    ];
    await microtasksFinished();

    const faviconItems =
        sourcesButton.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(faviconItems.length, 3);
    assertFalse(!!sourcesButton.shadowRoot.querySelector('.more-items'));
  });

  test('shows 3 tab icons with number for 4 tabs', async () => {
    const sourcesButton =
        topToolbar.shadowRoot.querySelector<ContextualTasksFaviconGroupElement>(
            '#sources');
    assertTrue(!!sourcesButton);

    topToolbar.attachedTabs = [
      {tabId: 1, title: 'Tab 1', url: {url: 'https://example.com/1'}},
      {tabId: 2, title: 'Tab 2', url: {url: 'https://example.com/2'}},
      {tabId: 3, title: 'Tab 3', url: {url: 'https://example.com/3'}},
      {tabId: 4, title: 'Tab 4', url: {url: 'https://example.com/4'}},
    ];
    await microtasksFinished();

    const faviconItems =
        sourcesButton.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(faviconItems.length, 3);
    const moreItems =
        sourcesButton.shadowRoot.querySelector<HTMLElement>('.more-items');
    assertTrue(!!moreItems);
    assertEquals(moreItems.textContent, '+1');
  });
});
