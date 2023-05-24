// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId, updateSubAppToParentAppId} from 'chrome://os-settings/os_settings.js';
import {convertOptionalBoolToBool, getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.js';

suite('<app-management-pwa-detail-view>', function() {
  let pwaDetailView;
  let fakeHandler;

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(pwaDetailView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(pwaDetailView, permissionType).click();
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

    pwaDetailView = document.createElement('app-management-pwa-detail-view');
    replaceBody(pwaDetailView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        pwaDetailView.app_.id);
  });

  test('toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(
          getPermissionCrToggleByType(pwaDetailView, permissionType).checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(
          getPermissionCrToggleByType(pwaDetailView, permissionType).checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(
          getPermissionCrToggleByType(pwaDetailView, permissionType).checked);
    };

    await checkToggle('kNotifications');
    await checkToggle('kLocation');
    await checkToggle('kCamera');
    await checkToggle('kMicrophone');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = pwaDetailView.$.pinToShelfSetting;
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

  test('Show sub apps correctly', async function() {
    const sub1 = await fakeHandler.addApp();
    const sub2 = await fakeHandler.addApp();
    const parent = await fakeHandler.addApp();
    AppManagementStore.getInstance().dispatch(
        updateSubAppToParentAppId(sub1.id, parent.id));
    AppManagementStore.getInstance().dispatch(
        updateSubAppToParentAppId(sub2.id, parent.id));

    await fakeHandler.flushPipesForTesting();

    const subAppsItem = pwaDetailView.$.subAppsItem;

    // Default app is shown, has neither parents nor sub apps.
    assertEquals(
        subAppsItem.subApps.length, 0, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');

    // Parent app with two sub apps gets selected.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(parent.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        subAppsItem.subApps.length, 2, 'list of sub apps should show two apps');
    assertFalse(subAppsItem.hidden, 'list of sub apps should not be hidden');

    // Select a sub app, has one parent and no sub apps of its own.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(sub1.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        subAppsItem.subApps.length, 0, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');
  });
});
