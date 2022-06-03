// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded} from 'chrome://history/history.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';

suite('#overflow-menu', function() {
  let listContainer;
  let sharedMenu;

  let target1;
  let target2;

  setup(function() {
    document.body.innerHTML = '';
    const testService = new TestBrowserService();
    BrowserService.setInstance(testService);

    const app = document.createElement('history-app');
    document.body.appendChild(app);
    return Promise
        .all([
          testService.whenCalled('queryHistory'),
          ensureLazyLoaded(),
        ])
        .then(function() {
          listContainer = app.$['history'];
          const element1 = document.createElement('div');
          const element2 = document.createElement('div');
          document.body.appendChild(element1);
          document.body.appendChild(element2);

          target1 = element1;
          target2 = element2;
          sharedMenu = listContainer.$.sharedMenu.get();
        });
  });

  test('opening and closing menu', function() {
    const detail1 = {target: target1};
    listContainer.dispatchEvent(new CustomEvent(
        'open-menu', {bubbles: true, composed: true, detail: detail1}));
    assertTrue(sharedMenu.open);
    assertEquals(detail1, listContainer.actionMenuModel_);

    sharedMenu.close();
    assertFalse(sharedMenu.open);

    const detail2 = {target: target2};
    listContainer.dispatchEvent(new CustomEvent(
        'open-menu', {bubbles: true, composed: true, detail: detail2}));
    assertEquals(detail2, listContainer.actionMenuModel_);
    assertTrue(sharedMenu.open);

    sharedMenu.close();
    assertFalse(sharedMenu.open);
  });

  teardown(function() {
    sharedMenu.actionMenuModel_ = null;
  });
});
