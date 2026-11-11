// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserProxyImpl} from 'chrome://history/history.js';
import type {HistoryEntry, QueryResult} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

// <if expr="is_android">
// clang-format off
import type {HistoryToolbarElement} from 'chrome://history/history.js';

import {assertPx} from './test_util.js';
// clang-format on
// </if>

suite('history-toolbar', function() {
  let app: HistoryAppElement;
  let testProxy: TestHistoryBrowserProxy;
  const TEST_HISTORY_RESULTS: [HistoryEntry] =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);

    app = document.createElement('history-app');
    document.body.appendChild(app);
    await testProxy.handler.whenCalled('queryHistory');
    return microtasksFinished();
  });

  test('selecting checkbox causes toolbar to change', async function() {
    testProxy.handler.setResultFor('queryHistoryContinuation', Promise.resolve({
      results: {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS},
    }));
    app.$.history.fire('query-history', true);
    await testProxy.handler.whenCalled('queryHistoryContinuation');
    await microtasksFinished();
    const item = app.$.history.shadowRoot.querySelector('history-item')!;
    item.$.checkbox.click();
    await microtasksFinished();

    const toolbar = app.$.toolbar;

    // Ensure that when an item is selected that the count held by the
    // toolbar increases.
    assertEquals(1, toolbar.count);
    assertTrue(toolbar.$.mainToolbar.hasAttribute('has-overlay'));

    item.$.checkbox.click();
    await item.$.checkbox.updateComplete;
    await microtasksFinished();

    // Ensure that when an item is deselected the count held by the
    // toolbar decreases.
    assertEquals(0, toolbar.count);
    assertFalse(toolbar.$.mainToolbar.hasAttribute('has-overlay'));
  });

  test('search term gathered correctly from toolbar', async function() {
    testProxy.handler.resetResolver('queryHistory');
    const toolbar = app.$.toolbar;
    testProxy.handler.setResultFor('queryHistory', Promise.resolve({
      results: {info: createHistoryInfo('Test'), value: TEST_HISTORY_RESULTS},
    }));
    toolbar.$.mainToolbar.fire('search-changed', 'Test');
    const queryArgs = await testProxy.handler.whenCalled('queryHistory');
    assertEquals('Test', queryArgs[0]);
  });

  test('spinner is active on search', async function() {
    testProxy.handler.resetResolver('queryHistory');

    const delayedQuery = new PromiseResolver<{results: QueryResult}>();

    testProxy.handler.setResultFor('queryHistory', delayedQuery.promise);

    const toolbar = app.$.toolbar;
    toolbar.$.mainToolbar.fire('search-changed', 'Test2');
    await testProxy.handler.whenCalled('queryHistory');
    await microtasksFinished();

    assertTrue(toolbar.spinnerActive);
    delayedQuery.resolve({
      results: {
        info: createHistoryInfo('Test2'),
        value: TEST_HISTORY_RESULTS,
      },
    });
    await microtasksFinished();
    assertFalse(toolbar.spinnerActive);
  });
});

// <if expr="is_android">
suite('history-toolbar-android', function() {
  const sideBarWidth = 256;
  const cardPadding = 16;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(new TestHistoryBrowserProxy());
    document.documentElement.style.setProperty(
        '--side-bar-width', `${sideBarWidth}px`);
    document.documentElement.style.setProperty(
        '--card-padding-side', `${cardPadding}px`);
  });

  teardown(function() {
    document.documentElement.style.removeProperty('--card-max-width');
    document.documentElement.style.removeProperty('--card-padding-side');
    document.documentElement.style.removeProperty('--side-bar-width');
  });

  // Creates a toolbar in wide mode, independent of the test window size.
  async function createToolbar(): Promise<HistoryToolbarElement> {
    const toolbar = document.createElement('history-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    toolbar.$.mainToolbar.narrowThreshold = 0;
    await microtasksFinished();
    assertFalse(toolbar.$.mainToolbar.narrow);
    // Skip the field's width transition so its box can be measured.
    toolbar.searchField.style.transition = 'none';
    return toolbar;
  }

  test('SearchFieldAndOverlayMatchCards', async function() {
    const cardWidth = 300;
    document.documentElement.style.setProperty(
        '--card-max-width', `${cardWidth}px`);
    const toolbar = await createToolbar();
    const toolbarRect = toolbar.getBoundingClientRect();
    const cardLeft =
        sideBarWidth + (toolbarRect.width - sideBarWidth - cardWidth) / 2;
    const fieldRect = toolbar.searchField.getBoundingClientRect();
    assertPx(cardLeft, fieldRect.left - toolbarRect.left);
    assertPx(cardLeft + cardWidth, fieldRect.right - toolbarRect.left);

    const overlay =
        toolbar.shadowRoot.querySelector('cr-toolbar-selection-overlay');
    assertTrue(!!overlay);
    assertPx(
        toolbar.$.mainToolbar.getBoundingClientRect().height,
        overlay.getBoundingClientRect().height);
    const content =
        overlay.shadowRoot.querySelector<HTMLElement>('#overlay-content');
    assertTrue(!!content);
    const contentRect = content.getBoundingClientRect();
    assertPx(cardLeft - cardPadding, contentRect.left - toolbarRect.left);
    assertPx(
        cardLeft + cardWidth + cardPadding,
        contentRect.right - toolbarRect.left);
  });

  test('SearchFieldShrinksWithCardColumn', async function() {
    document.documentElement.style.setProperty('--card-max-width', '600px');
    const toolbar = await createToolbar();
    // Leave less room beside the side bar than the cards' max width.
    toolbar.style.width = `${sideBarWidth + 400}px`;
    await microtasksFinished();
    const toolbarRect = toolbar.getBoundingClientRect();
    const fieldRect = toolbar.searchField.getBoundingClientRect();
    assertPx(sideBarWidth + cardPadding, fieldRect.left - toolbarRect.left);
    assertPx(
        toolbarRect.width - cardPadding, fieldRect.right - toolbarRect.left);
  });

  test('SearchFieldIsCenteredInToolbar', async function() {
    const toolbar = await createToolbar();
    const toolbarRect = toolbar.$.mainToolbar.getBoundingClientRect();
    const wideFieldRect = toolbar.searchField.getBoundingClientRect();
    assertPx(
        toolbarRect.top + toolbarRect.height / 2,
        wideFieldRect.top + wideFieldRect.height / 2);

    toolbar.$.mainToolbar.narrow = true;
    await microtasksFinished();
    const narrowFieldRect = toolbar.searchField.getBoundingClientRect();
    assertPx(
        toolbarRect.top + toolbarRect.height / 2,
        narrowFieldRect.top + narrowFieldRect.height / 2);
    assertTrue(
        narrowFieldRect.height < wideFieldRect.height,
        `narrow: ${narrowFieldRect.height}, wide: ${wideFieldRect.height}`);
  });
});
// </if>
