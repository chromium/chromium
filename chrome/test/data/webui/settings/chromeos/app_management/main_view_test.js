// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-main-view>', function() {
  let mainView;
  let fakeHandler;
  let store;

  /**
   * @param {number} numApps
   */
  async function addApps(numApps) {
    for (let i = 0; i < numApps; i++) {
      await fakeHandler.addApp();
    }
  }

  function getAppItems() {
    return mainView.$$('app-management-expandable-app-list')
        .querySelectorAll('app-management-app-item');
  }

  setup(function() {
    fakeHandler = setupFakeHandler();
    store = replaceStore();

    mainView = document.createElement('app-management-main-view');
    replaceBody(mainView);
  });

  test('simple app addition', async function() {
    // Ensure there is no apps initially
    expectEquals(0, getAppItems().length);

    const app = await fakeHandler.addApp();

    const appItems = getAppItems();
    expectEquals(1, appItems.length);
    expectEquals(app.id, appItems[0].app.id);

    store.setReducersEnabled(false);
    appItems[0].click();
    const expected = app_management.actions.changePage(PageType.DETAIL, app.id);
    assertDeepEquals(expected, store.lastAction);
  });

  // TODO(jshikaram) uncomment once more apps bar is added back.
  test.skip('more apps bar visibility', async function() {
    // The more apps bar shouldn't appear when there are 4 apps.
    await addApps(NUMBER_OF_APPS_DISPLAYED_DEFAULT);
    expectEquals(NUMBER_OF_APPS_DISPLAYED_DEFAULT, getAppItems().length);
    expectTrue(mainView.$$('app-management-expandable-app-list')
                   .$['expander-row']
                   .hidden);

    // The more apps bar appears when there are 5 apps.
    await addApps(1);
    expectEquals(NUMBER_OF_APPS_DISPLAYED_DEFAULT + 1, getAppItems().length);
    expectFalse(mainView.$$('app-management-expandable-app-list')
                    .$['expander-row']
                    .hidden);
  });

  test('notifications sublabel collapsibility', async function() {
    // The three spans contains collapsible attribute.
    await addApps(4);
    const pieces = await mainView.getNotificationSublabelPieces_();
    expectTrue(pieces.filter(p => p.arg === '$1')[0].collapsible);
    expectTrue(pieces.filter(p => p.arg === '$2')[0].collapsible);
    expectTrue(pieces.filter(p => p.arg === '$3')[0].collapsible);

    // Checking ",and other x apps" is non-collapsible
    await addApps(6);
    const pieces2 = await mainView.getNotificationSublabelPieces_();
    expectFalse(pieces2.filter(p => p.arg === '$4')[0].collapsible);
  });
});
