// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('drawer-test', function() {
  let app;

  setup(function() {
    document.body.innerHTML = '';
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
    app.selectedPage_ = 'syncedTabs';
    app.hasDrawer_ = true;
    return flushTasks().then(function() {
      const drawer = /** @type {CrLazyRenderElement} */ (app.$.drawer);
      let drawerSideBar = app.$$('#drawer-side-bar');

      assertTrue(!!drawer);
      // Drawer side bar doesn't exist until the first time the drawer is
      // opened.
      assertFalse(!!drawerSideBar);

      const menuButton =
          app.$.toolbar.$['main-toolbar'].shadowRoot.querySelector(
              '#menuButton');
      assertTrue(!!menuButton);

      menuButton.click();
      assertTrue(drawer.getIfExists().open);
      drawerSideBar = app.$$('#drawer-side-bar');
      assertTrue(!!drawerSideBar);

      assertEquals('syncedTabs', drawerSideBar.$.menu.selected);
    });
  });
});
