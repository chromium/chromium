// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, convertOptionalBoolToBool} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://test/test_util.js';

suite('<app-management-pin-to-shelf-item>', () => {
  let pinToShelfItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    pinToShelfItem = document.createElement('app-management-pin-to-shelf-item');

    replaceBody(pinToShelfItem);
    flushTasks();
  });

  test('Toggle pin to shelf', async () => {
    const arcOptions = {
      type: appManagement.mojom.AppType.kArc,
      permissions: {},
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', arcOptions);

    await fakeHandler.flushPipesForTesting();
    pinToShelfItem.app = app;
    assertFalse(convertOptionalBoolToBool(
        AppManagementStore.getInstance().data.apps[app.id].isPinned));

    pinToShelfItem.click();
    flushTasks();
    await fakeHandler.flushPipesForTesting();

    assertTrue(convertOptionalBoolToBool(
        AppManagementStore.getInstance().data.apps[app.id].isPinned));
  });
});
