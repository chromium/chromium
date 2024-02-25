// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementMainViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementActions, AppManagementPageState, Router} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestStore} from 'chrome://webui-test/test_store.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-main-view>', () => {
  let mainView: AppManagementMainViewElement;
  let fakeHandler: FakePageHandler;
  let store: TestStore<AppManagementPageState, AppManagementActions>;

  function getAppItems() {
    const element = mainView.shadowRoot!.querySelector('#appList');
    assertTrue(!!element);
    return element.querySelectorAll('app-management-app-item');
  }

  setup(() => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();

    mainView = document.createElement('app-management-main-view');
    replaceBody(mainView);
  });

  teardown(() => {
    mainView.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('simple app addition', async () => {
    // Ensure there is no apps initially
    assertEquals(0, getAppItems().length);

    await fakeHandler.addApp();

    const appItems = getAppItems();
    assertTrue(!!appItems);
    assertEquals(1, appItems.length);
    assertNull(Router.getInstance().getQueryParameters().get('id'));
    store.setReducersEnabled(false);

    appItems[0]!.click();
    fakeHandler.flushPipesForTesting();
    assertTrue(!!Router.getInstance().getQueryParameters().get('id'));
  });
});
