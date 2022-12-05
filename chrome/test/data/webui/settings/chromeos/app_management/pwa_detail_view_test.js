// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId, getPermissionValueBool, convertOptionalBoolToBool} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.js';

suite('<app-management-pwa-detail-view>', function() {
  let pwaPermissionView;
  let fakeHandler;

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(pwaPermissionView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(pwaPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = AppManagementStore.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    const app = await fakeHandler.addApp();
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    pwaPermissionView =
        document.createElement('app-management-pwa-detail-view');
    replaceBody(pwaPermissionView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
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

    await checkToggle('kNotifications');
    await checkToggle('kLocation');
    await checkToggle('kCamera');
    await checkToggle('kMicrophone');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = pwaPermissionView.$.pinToShelfSetting;
    const toggle = pinToShelfItem.$.toggleRow.$.toggle;

    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
  });
});
