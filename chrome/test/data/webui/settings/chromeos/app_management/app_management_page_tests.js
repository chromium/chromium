// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('AppManagementPageTests', () => {
  let appManagementPage;
  let fakeHandler;
  let store;

  /** @return {Element} */
  function getAppList() {
    return appManagementPage.$$('app-management-main-view').$['app-list'];
  }

  /** @return {number} */
  function getAppListChildren() {
    return getAppList().childElementCount -
        1;  // Ignore the dom-repeat element.
  }

  /** @return {Element} */
  function getNoAppsFoundLabel() {
    return appManagementPage.$$('app-management-main-view')
        .$$('#no-apps-label');
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();
    appManagementPage = document.createElement('settings-app-management-page');
    assertTrue(!!appManagementPage);
    replaceBody(appManagementPage);
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
  });

  test('App list renders on page change', async () => {
    await fakeHandler.addApp();
    expectEquals(1, getAppListChildren());
    await fakeHandler.addApp();
    expectEquals(2, getAppListChildren());
  });

  test('No Apps Found Label', async () => {
    expectEquals(0, getAppListChildren());
    expectFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    const app = await fakeHandler.addApp();
    expectEquals(1, getAppListChildren());
    expectTrue(isHiddenByDomIf(getNoAppsFoundLabel()));

    fakeHandler.uninstall(app.id);
    await test_util.flushTasks();
    expectEquals(0, getAppListChildren());
    expectFalse(isHiddenByDomIf(getNoAppsFoundLabel()));
  });

  test('App list filters when searching', async () => {
    await fakeHandler.addApp(null, {title: 'slides'});
    await fakeHandler.addApp(null, {title: 'calculator'});
    const sheets = await fakeHandler.addApp(null, {title: 'sheets'});
    expectEquals(3, getAppListChildren());

    appManagementPage.searchTerm = 's';
    await test_util.flushTasks();
    expectEquals(2, getAppListChildren());

    fakeHandler.uninstall(sheets.id);
    await test_util.flushTasks();
    expectEquals(1, getAppListChildren());

    appManagementPage.searchTerm = 'ss';
    await test_util.flushTasks();
    expectEquals(0, getAppListChildren());
    expectFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    appManagementPage.searchTerm = '';
    await test_util.flushTasks();
    expectEquals(2, getAppListChildren());
  });
});
