// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {BorealisPermissionType, createPermission, PermissionValueType, Bool, AppManagementStore, updateSelectedAppId, getPermissionValueBool, convertOptionalBoolToBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-borealis-detail-view>', function() {
  let BorealisDetailView;
  let fakeHandler;

  const kBorealisMainAppId = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        BorealisDetailView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(BorealisDetailView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = app_management.AppManagementStore.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const permissions = {};
    const permissionIds = [BorealisPermissionType.MICROPHONE];
    for (const permissionId of permissionIds) {
      permissions[permissionId] = app_management.util.createPermission(
          permissionId, PermissionValueType.kBool, Bool.kTrue,
          false /*is_managed*/);
    }

    // Add an app, and make it the currently selected app.
    const options = {
      type: apps.mojom.AppType.kBorealis,
      permissions: permissions
    };
    const app = await fakeHandler.addApp(kBorealisMainAppId, options);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    BorealisDetailView =
        document.createElement('app-management-borealis-detail-view');
    replaceBody(BorealisDetailView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        BorealisDetailView.app_.id);
  });

  test('Toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(BorealisDetailView, permissionType)
                     .checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(
          getPermissionCrToggleByType(BorealisDetailView, permissionType)
              .checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(BorealisDetailView, permissionType)
                     .checked);
    };

    await checkToggle('MICROPHONE');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = BorealisDetailView.$['pin-to-shelf-setting'];
    const toggle = pinToShelfItem.$['toggle-row'].$.toggle;

    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
  });
});
