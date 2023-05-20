// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {createBoolPermission, createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden, getPermissionItemByType, getPermissionCrToggleByType} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {AppType, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {FakePageHandler} from './fake_page_handler.js';


suite('<app-management-arc-detail-view>', () => {
  let arcPermissionView;
  let fakeHandler;

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(arcPermissionView.app_, permissionType);
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

    // Create an ARC app without microphone permissions.
    const arcOptions = {
      type: AppType.kArc,
      permissions: FakePageHandler.createArcPermissions([
        PermissionType.kCamera,
        PermissionType.kLocation,
        PermissionType.kNotifications,
        PermissionType.kContacts,
        PermissionType.kStorage,
      ]),
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    arcPermissionView =
        document.createElement('app-management-arc-detail-view');
    replaceBody(arcPermissionView);
    await flushTasks();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        arcPermissionView.app_.id);
  });

  test('Permissions are hidden correctly', () => {
    assertTrue(
        isHidden(getPermissionItemByType(arcPermissionView, 'kMicrophone')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kLocation')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kCamera')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kStorage')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kCamera')));
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

    await checkPermissionToggle('kLocation');
    await checkPermissionToggle('kCamera');
    await checkPermissionToggle('kNotifications');
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

    await checkPermissionItemOnClick('kLocation');
    await checkPermissionItemOnClick('kCamera');
    await checkPermissionItemOnClick('kNotifications');
    await checkPermissionItemOnClick('kContacts');
    await checkPermissionItemOnClick('kStorage');
  });

  test('No permissions requested label', async () => {
    assertTrue(isHiddenByDomIf(
        arcPermissionView.shadowRoot.querySelector('#noPermissions')));

    // Create an ARC app without any permissions.
    const arcOptions = {
      type: AppType.kArc,
      permissions: FakePageHandler.createArcPermissions([]),
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    await flushTasks();

    assertFalse(isHiddenByDomIf(
        arcPermissionView.shadowRoot.querySelector('#noPermissions')));
  });

  suite('Read-only permissions', () => {
    setup(async () => {
      loadTimeData.overrideValues(
          {'appManagementArcReadOnlyPermissions': true});

      // Re-render with the new loadTimeData.
      arcPermissionView =
          document.createElement('app-management-arc-detail-view');
      replaceBody(arcPermissionView);
      await flushTasks();
    });

    teardown(() => {
      loadTimeData.overrideValues(
          {'appManagementArcReadOnlyPermissions': false});
    });

    test('Boolean permission display', async () => {
      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');
      assertEquals(
          'app-management-read-only-permission-item',
          locationItem.tagName.toLowerCase());

      assertEquals(
          'Allowed',
          locationItem.shadowRoot.querySelector('#description')
              .textContent.trim());

      // Simulate the permission being changed by the OS, and verify that the
      // description text updates.
      fakeHandler.setPermission(
          arcPermissionView.app_.id,
          createBoolPermission(
              PermissionType.kLocation, /*value=*/ false,
              /*is_managed=*/ false));
      await flushTasks();

      assertEquals(
          'Denied',
          locationItem.shadowRoot.querySelector('#description')
              .textContent.trim());
    });

    test('Tri-state permission display', async () => {
      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');

      fakeHandler.setPermission(
          arcPermissionView.app_.id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kAllow,
              /*is_managed=*/ false));
      await flushTasks();

      assertEquals(
          'Allowed',
          locationItem.shadowRoot.querySelector('#description')
              .textContent.trim());

      fakeHandler.setPermission(
          arcPermissionView.app_.id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kAsk,
              /*is_managed=*/ false));
      await flushTasks();

      assertEquals(
          'Ask every time',
          locationItem.shadowRoot.querySelector('#description')
              .textContent.trim());

      fakeHandler.setPermission(
          arcPermissionView.app_.id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kBlock,
              /*is_managed=*/ false));
      await flushTasks();

      assertEquals(
          'Denied',
          locationItem.shadowRoot.querySelector('#description')
              .textContent.trim());
    });


    test('Permission display with detail', async () => {
      const permission = createBoolPermission(
          PermissionType.kLocation, /*value=*/ true,
          /*is_managed=*/ false);
      permission.details = 'While in use';

      fakeHandler.setPermission(arcPermissionView.app_.id, permission);
      await flushTasks();

      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');

      assertTrue(locationItem.shadowRoot.querySelector('#description')
                     .textContent.includes('While in use'));

      permission.value.boolValue = false;

      fakeHandler.setPermission(arcPermissionView.app_.id, permission);
      await flushTasks();

      assertFalse(locationItem.shadowRoot.querySelector('#description')
                      .textContent.includes('While in use'));
    });
  });

});
