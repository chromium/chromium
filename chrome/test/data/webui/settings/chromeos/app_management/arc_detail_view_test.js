// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, updateArcSupported, FakePageHandler, ArcPermissionType, updateSelectedAppId, getPermissionValueBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden, getPermissionItemByType, getPermissionCrToggleByType} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-arc-detail-view>', () => {
  let arcPermissionView;
  let fakeHandler;

  function expandPermissions() {
    arcPermissionView.root.querySelector('#subpermission-expand-row').click();
  }

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        arcPermissionView.app_, permissionType);
  }

  async function clickPermissionToggle(permissionType) {
    getPermissionCrToggleByType(arcPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  async function clickPermissionItem(permissionType) {
    getPermissionItemByType(arcPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateArcSupported(true));

    // Create an ARC app without microphone permissions.
    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      permissions: app_management.FakePageHandler.createArcPermissions([
        ArcPermissionType.CAMERA,
        ArcPermissionType.LOCATION,
        ArcPermissionType.NOTIFICATIONS,
        ArcPermissionType.CONTACTS,
        ArcPermissionType.STORAGE,
      ])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    arcPermissionView =
        document.createElement('app-management-arc-detail-view');
    replaceBody(arcPermissionView);
  });

  test('App is rendered correctly', () => {
    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        arcPermissionView.app_.id);
  });

  test('Permissions are hidden correctly', () => {
    expandPermissions();
    assertTrue(
        isHidden(getPermissionItemByType(arcPermissionView, 'MICROPHONE')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'LOCATION')));
    assertFalse(isHidden(getPermissionItemByType(arcPermissionView, 'CAMERA')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'STORAGE')));
    assertFalse(isHidden(getPermissionItemByType(arcPermissionView, 'CAMERA')));
  });

  test('Toggle works correctly', async () => {
    const checkPermissionToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);

      // Toggle Off.
      await clickPermissionToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionCrToggleByType(arcPermissionView, permissionType)
                      .checked);

      // Toggle On.
      await clickPermissionToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);
    };

    expandPermissions();
    await checkPermissionToggle('LOCATION');
    await checkPermissionToggle('CAMERA');
    await checkPermissionToggle('NOTIFICATIONS');
  });


  test('OnClick handler for permission item works correctly', async () => {
    const checkPermissionItemOnClick = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);

      // Toggle Off.
      await clickPermissionItem(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionCrToggleByType(arcPermissionView, permissionType)
                      .checked);

      // Toggle On.
      await clickPermissionItem(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);
    };

    expandPermissions();
    await checkPermissionItemOnClick('LOCATION');
    await checkPermissionItemOnClick('CAMERA');
    await checkPermissionItemOnClick('NOTIFICATIONS');
    await checkPermissionItemOnClick('CONTACTS');
    await checkPermissionItemOnClick('STORAGE');
  });

  test('Unsupported Arc hides correctly', () => {
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'NOTIFICATIONS')));
    assertFalse(
        isHidden(arcPermissionView.root.getElementById('permissions-card')));

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateArcSupported(false));

    assertTrue(
        isHidden(getPermissionItemByType(arcPermissionView, 'NOTIFICATIONS')));
    assertTrue(
        isHidden(arcPermissionView.root.getElementById('permissions-card')));

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateArcSupported(true));
  });

  test('No permissions requested label', async () => {
    expectTrue(isHiddenByDomIf(
        arcPermissionView.root.querySelector('#no-permissions')));

    // Create an ARC app without any permissions.
    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      permissions: app_management.FakePageHandler.createArcPermissions([])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));
    await test_util.flushTasks();

    expectFalse(isHiddenByDomIf(
        arcPermissionView.root.querySelector('#no-permissions')));
  });
});
