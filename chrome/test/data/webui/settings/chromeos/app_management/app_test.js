// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-app>', () => {
  let app;
  let fakeHandler;
  let store;

  /** @return {ExpandableAppList} */
  function getAppList() {
    return app.shadowRoot.querySelector('app-management-main-view')
        .shadowRoot.querySelector('app-management-expandable-app-list');
  }

  /** @param {String} term  */
  async function searchApps(term) {
    app.dispatch(app_management.actions.setSearchTerm(term));
    await test_util.flushTasks();
  }

  /** @return {boolean} */
  function isSearchViewShown() {
    return !!app.shadowRoot.querySelector('app-management-search-view');
  }

  /** @return {boolean} */
  function isMainViewShown() {
    return !!app.shadowRoot.querySelector('app-management-main-view');
  }

  /** @return {boolean} */
  function isDetailViewShown() {
    return !!app.shadowRoot.querySelector('app-management-pwa-detail-view');
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();
    app = document.createElement('app-management-app');
    replaceBody(app);
    await app.shadowRoot.querySelector('app-management-dom-switch')
        .firstRenderForTesting_.promise;
    await test_util.flushTasks();
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
  });

  test('Searching switches to search page', async () => {
    app.shadowRoot.querySelector('cr-toolbar')
        .fire('search-changed', 'SearchTest');
    assert(app.shadowRoot.querySelector('app-management-search-view'));
  });

  test('App list renders on page change', (done) => {
    const appList = getAppList();
    let numApps = 0;

    fakeHandler.addApp()
        .then(() => {
          numApps = 1;
          assertEquals(numApps, appList.numChildrenForTesting_);

          // Click app to go to detail page.
          appList.querySelector('app-management-app-item').click();
          return test_util.flushTasks();
        })
        .then(() => {
          return fakeHandler.addApp();
        })
        .then(() => {
          numApps++;

          appList.addEventListener('num-children-for-testing_-changed', () => {
            assertEquals(numApps, appList.numChildrenForTesting_);
            done();
          });

          // Click back button to go to main page.
          app.shadowRoot.querySelector('app-management-pwa-detail-view')
              .shadowRoot.querySelector('app-management-detail-view-header')
              .shadowRoot.querySelector('#backButton')
              .click();
          test_util.flushTasks();
        });
  });

  test('Search from main page', async () => {
    await navigateTo('/');
    assertTrue(isMainViewShown());

    await searchApps('o');
    assertTrue(isSearchViewShown());
    assertEquals('/?q=o', getCurrentUrlSuffix());

    await searchApps('');
    assertTrue(isMainViewShown());
    assertEquals('/', getCurrentUrlSuffix());
  });

  test('Search from detail page', async () => {
    await fakeHandler.addApp();

    await navigateTo('/detail?id=0');
    assertTrue(isDetailViewShown());

    await searchApps('o');
    assertTrue(isSearchViewShown());
    assertEquals('/detail?id=0&q=o', getCurrentUrlSuffix());

    await searchApps('');
    assertTrue(isDetailViewShown());
    assertEquals('/detail?id=0', getCurrentUrlSuffix());
  });
});
