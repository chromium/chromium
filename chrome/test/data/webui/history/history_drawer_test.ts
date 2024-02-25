// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement, HistorySideBarElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {navigateTo} from './test_util.js';

suite('drawer-test', function() {
  let app: HistoryAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    app = document.createElement('history-app');
    document.body.appendChild(app);
    return Promise.all([
      testService.whenCalled('queryHistory'),
      ensureLazyLoaded(),
    ]);
  });

  test('drawer has correct selection', function() {
    navigateTo('/syncedTabs', app);
    app.setHasDrawerForTesting(true);
    return flushTasks().then(function() {
      const drawerLazyRender = app.$.drawer;
      assertTrue(!!drawerLazyRender);

      // Drawer side bar doesn't exist until the first time the drawer is
      // opened.
      let drawerSideBar = app.shadowRoot!.querySelector<HistorySideBarElement>(
          '#drawer-side-bar');
      assertFalse(!!drawerSideBar);

      const menuButton =
          app.$.toolbar.$.mainToolbar.shadowRoot!.querySelector<HTMLElement>(
              '#menuButton');
      assertTrue(!!menuButton);

      menuButton.click();
      const drawer = drawerLazyRender.getIfExists();
      assertTrue(!!drawer);
      assertTrue(drawer.open);
      drawerSideBar = app.shadowRoot!.querySelector<HistorySideBarElement>(
          '#drawer-side-bar');
      assertTrue(!!drawerSideBar);

      assertEquals('syncedTabs', drawerSideBar.$.menu.selected);
    });
  });
});
