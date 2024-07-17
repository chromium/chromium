// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSetsAppElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {RelatedWebsiteSetsApiBrowserProxyImpl} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestRelatedWebsiteSetsApiBrowserProxy} from './test_api_proxy.js';

suite('AppTest', () => {
  let app: RelatedWebsiteSetsAppElement;
  let testProxy: TestRelatedWebsiteSetsApiBrowserProxy;

  setup(async () => {
    testProxy = new TestRelatedWebsiteSetsApiBrowserProxy();
    RelatedWebsiteSetsApiBrowserProxyImpl.setInstance(testProxy);

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
});
