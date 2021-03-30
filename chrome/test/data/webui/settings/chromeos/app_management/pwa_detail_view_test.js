// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, updateSelectedAppId, getPermissionValueBool, convertOptionalBoolToBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-pwa-detail-view>', function() {
  let pwaPermissionView;
  let fakeHandler;

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        pwaPermissionView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(pwaPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = app_management.AppManagementStore.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    const app = await fakeHandler.addApp();
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    pwaPermissionView =
        document.createElement('app-management-pwa-detail-view');
    replaceBody(pwaPermissionView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        pwaPermissionView.app_.id);
  });

  test('toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(pwaPermissionView, permissionType)
                     .checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionCrToggleByType(pwaPermissionView, permissionType)
                      .checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(pwaPermissionView, permissionType)
                     .checked);
    };

    await checkToggle('NOTIFICATIONS');
    await checkToggle('GEOLOCATION');
    await checkToggle('MEDIASTREAM_CAMERA');
    await checkToggle('MEDIASTREAM_MIC');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = pwaPermissionView.$['pin-to-shelf-setting'];
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
