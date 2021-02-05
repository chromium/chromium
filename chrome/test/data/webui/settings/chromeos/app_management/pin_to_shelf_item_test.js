// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, updateArcSupported, convertOptionalBoolToBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-pin-to-shelf-item>', () => {
  let pinToShelfItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    pinToShelfItem = document.createElement('app-management-pin-to-shelf-item');

    replaceBody(pinToShelfItem);
    test_util.flushTasks();
  });

  test('Toggle pin to shelf', async () => {
    const arcOptions = {type: apps.mojom.AppType.kArc, permissions: {}};

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateArcSupported(true));

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', arcOptions);

    await fakeHandler.flushPipesForTesting();
    pinToShelfItem.app = app;
    assertFalse(app_management.util.convertOptionalBoolToBool(
        app_management.AppManagementStore.getInstance()
            .data.apps[app.id]
            .isPinned));

    pinToShelfItem.click();
    test_util.flushTasks();
    await fakeHandler.flushPipesForTesting();

    assertTrue(app_management.util.convertOptionalBoolToBool(
        app_management.AppManagementStore.getInstance()
            .data.apps[app.id]
            .isPinned));
  });
});
