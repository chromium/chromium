// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementPinToShelfItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore} from 'chrome://os-settings/os_settings.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-pin-to-shelf-item>', () => {
  let pinToShelfItem: AppManagementPinToShelfItemElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    pinToShelfItem = document.createElement('app-management-pin-to-shelf-item');

    replaceBody(pinToShelfItem);
    flushTasks();
  });

  teardown(() => {
    pinToShelfItem.remove();
  });

  test('Toggle pin to shelf', async () => {
    const arcOptions = {
      type: AppType.kArc,
      permissions: {},
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', arcOptions);

    await fakeHandler.flushPipesForTesting();
    pinToShelfItem.app = app;
    let selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertFalse(!!selectedApp.isPinned);

    pinToShelfItem.click();
    flushTasks();
    await fakeHandler.flushPipesForTesting();

    selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertTrue(!!selectedApp.isPinned);
  });
});
