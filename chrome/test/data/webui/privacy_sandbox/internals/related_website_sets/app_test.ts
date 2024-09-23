// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSetsAppElement, RelatedWebsiteSetsListContainerElement, RelatedWebsiteSetsToolbarElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {RelatedWebsiteSetsApiBrowserProxyImpl} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestRelatedWebsiteSetsApiBrowserProxy} from './test_api_proxy.js';
import {GetRelatedWebsiteSetsResponseForTest} from './test_data.js';

suite('AppTest', () => {
  let app: RelatedWebsiteSetsAppElement;
  let testProxy: TestRelatedWebsiteSetsApiBrowserProxy;

  setup(async () => {
    testProxy = new TestRelatedWebsiteSetsApiBrowserProxy();
    RelatedWebsiteSetsApiBrowserProxyImpl.setInstance(testProxy);
    testProxy.handler.relatedWebsiteSetsInfo =
        GetRelatedWebsiteSetsResponseForTest;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('related-website-sets-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    await microtasksFinished();
  });

  test('check initial state', async () => {
    assertEquals(1, testProxy.handler.getCallCount('getRelatedWebsiteSets'));
    assertTrue(isVisible(app.$.toolbar));
    assertTrue(isVisible(app.$.sidebar));
  });

  test('app drawer', async () => {
    app.setNarrowForTesting(true);
    await microtasksFinished();

    assertFalse(app.$.drawer.open);
    const menuButton =
        app.$.toolbar.$.mainToolbar.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(isVisible(menuButton));
    menuButton!.click();
    await microtasksFinished();

    // Verify drawer items are accurate
    assertTrue(app.$.drawer.open);
    const menuItems = app.$.sidebar.getMenuItemsForTesting();
    const drawerSidebar =
        app.$.drawer.querySelector('related-website-sets-sidebar');
    const drawerItems = drawerSidebar!.$.selector.children;
    assertEquals(menuItems.length, drawerItems.length);
    for (const [i, menuItem] of menuItems.entries()) {
      assertEquals(menuItem.name, drawerItems[i]!.textContent!.trim());
    }
    app.$.drawer.close();
    await microtasksFinished();

    assertFalse(isVisible(app.$.drawer));
  });

  test('URL parameters update UI search box', async () => {
    const query = new URLSearchParams(window.location.search);
    query.set('q', 'test');
    window.history.replaceState(
        {}, '', `${window.location.pathname}?${query.toString()}`);
    window.dispatchEvent(new CustomEvent('popstate'));
    await microtasksFinished();
    assertEquals(
        'test', app.$.toolbar.$.mainToolbar.getSearchField().getValue());
  });

  test('UI search box updates URL parameters', async () => {
    app.$.toolbar.$.mainToolbar.getSearchField().setValue('hello');
    const urlParams = new URLSearchParams(window.location.search);
    const query = urlParams.get('q');
    assertEquals('hello', query);
  });

  test('propagates query to child element', async () => {
    const expectedQuery = 'set2-';
    const toolbar = $$<RelatedWebsiteSetsToolbarElement>(app, '#toolbar');
    assertTrue(!!toolbar);
    toolbar.setSearchFieldValue(expectedQuery);
    await microtasksFinished();
    const contentContainer =
        app.shadowRoot!.querySelector<RelatedWebsiteSetsListContainerElement>(
            '#content > related-website-sets-list-container');
    assertTrue(!!contentContainer);
    const actualQuery = contentContainer.query;
    assertEquals(expectedQuery, actualQuery);
  });
});
