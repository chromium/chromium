// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, PermissionType, updateSelectedAppId, getPermissionValueBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// clang-format on

'use strict';

suite('<app-management-permission-item>', () => {
  let permissionItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      permissions: app_management.FakePageHandler.createArcPermissions([
        PermissionType.kCamera,
        PermissionType.kLocation,
        PermissionType.kNotifications,
        PermissionType.kContacts,
        PermissionType.kStorage,
      ])
    };

    // Add an arc app, and pass it to permissionItem.
    const app = await fakeHandler.addApp(null, arcOptions);

    permissionItem = document.createElement('app-management-permission-item');
    permissionItem.app_ = app;
  });

  test('Toggle permission', async () => {
    permissionItem.permissionType = 'kLocation';

    replaceBody(permissionItem);
    await fakeHandler.flushPipesForTesting();
    assertTrue(getPermissionValueBool(
        permissionItem.app_, permissionItem.permissionType));

    permissionItem.click();
    await test_util.flushTasks();
    await fakeHandler.flushPipesForTesting();
    // Store gets updated permission.
    const storeData = app_management.AppManagementStore.getInstance().data;
    assertFalse(getPermissionValueBool(
      storeData.apps[permissionItem.app_.id], permissionItem.permissionType));
  });
});
