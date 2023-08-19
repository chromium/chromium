// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementArcDetailViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, CrToggleElement, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermission, createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {getPermissionCrToggleByType, getPermissionItemByType, isHidden, isHiddenByDomIf, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

// TODO(b/270728282) - remove "as" cast once getPermissionCrToggleByType()
// becomes a TS function.
suite('<app-management-arc-detail-view>', () => {
  let arcPermissionView: AppManagementArcDetailViewElement;
  let fakeHandler: FakePageHandler;

  function getPermissionBoolByType(permissionType: PermissionTypeIndex):
      boolean {
    return getPermissionValueBool(
        arcPermissionView.get('app_'), permissionType);
  }

  async function clickPermissionToggle(permissionType: PermissionTypeIndex):
      Promise<void> {
    (getPermissionCrToggleByType(arcPermissionView, permissionType) as
     CrToggleElement)
        .click();
    await fakeHandler.flushPipesForTesting();
  }

  async function clickPermissionItem(permissionType: PermissionTypeIndex):
      Promise<void> {
    (getPermissionItemByType(arcPermissionView, permissionType) as HTMLElement)
        .click();
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
    const app = await fakeHandler.addApp('', arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    arcPermissionView =
        document.createElement('app-management-arc-detail-view');
    replaceBody(arcPermissionView);
    await flushTasks();
  });

  teardown(() => {
    arcPermissionView.remove();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        arcPermissionView.get('app_').id);
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

  test('No permissions requested label', async () => {
    assertTrue(isHiddenByDomIf(
        arcPermissionView.shadowRoot!.querySelector<HTMLElement>(
            '#noPermissions')));

    // Create an ARC app without any permissions.
    const arcOptions = {
      type: AppType.kArc,
      permissions: FakePageHandler.createArcPermissions([]),
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp('', arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    await flushTasks();

    assertFalse(isHiddenByDomIf(
        arcPermissionView.shadowRoot!.querySelector<HTMLElement>(
            '#noPermissions')));
  });

  suite('Read-write permissions', () => {
    setup(async () => {
      loadTimeData.overrideValues(
          {'appManagementArcReadOnlyPermissions': false});

      // Re-render with the new loadTimeData.
      arcPermissionView =
          document.createElement('app-management-arc-detail-view');
      replaceBody(arcPermissionView);
      await flushTasks();
    });

    teardown(() => {
      arcPermissionView.remove();
    });

    test('Toggle works correctly', async () => {
      const checkPermissionToggle =
          async (permissionType: PermissionTypeIndex) => {
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue((getPermissionCrToggleByType(
                        arcPermissionView, permissionType) as CrToggleElement)
                       .checked);

        // Toggle Off.
        await clickPermissionToggle(permissionType);
        assertFalse(getPermissionBoolByType(permissionType));
        assertFalse((getPermissionCrToggleByType(
                         arcPermissionView, permissionType) as CrToggleElement)
                        .checked);

        // Toggle On.
        await clickPermissionToggle(permissionType);
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue((getPermissionCrToggleByType(
                        arcPermissionView, permissionType) as CrToggleElement)
                       .checked);
      };

      await checkPermissionToggle('kLocation');
      await checkPermissionToggle('kCamera');
      await checkPermissionToggle('kNotifications');
    });

    test('OnClick handler for permission item works correctly', async () => {
      const checkPermissionItemOnClick =
          async (permissionType: PermissionTypeIndex) => {
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue((getPermissionCrToggleByType(
                        arcPermissionView, permissionType) as CrToggleElement)
                       .checked);

        // Toggle Off.
        await clickPermissionItem(permissionType);
        assertFalse(getPermissionBoolByType(permissionType));
        assertFalse((getPermissionCrToggleByType(
                         arcPermissionView, permissionType) as CrToggleElement)
                        .checked);

        // Toggle On.
        await clickPermissionItem(permissionType);
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue((getPermissionCrToggleByType(
                        arcPermissionView, permissionType) as CrToggleElement)
                       .checked);
      };

      await checkPermissionItemOnClick('kLocation');
      await checkPermissionItemOnClick('kCamera');
      await checkPermissionItemOnClick('kNotifications');
      await checkPermissionItemOnClick('kContacts');
      await checkPermissionItemOnClick('kStorage');
    });
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
      arcPermissionView.remove();
      loadTimeData.overrideValues(
          {'appManagementArcReadOnlyPermissions': false});
    });

    test('Boolean permission display', async () => {
      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');
      assertTrue(!!locationItem);
      assertEquals(
          'app-management-read-only-permission-item',
          locationItem.tagName.toLowerCase());

      let description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertEquals('Allowed', description.textContent!.trim());

      // Simulate the permission being changed by the OS, and verify that the
      // description text updates.
      fakeHandler.setPermission(
          arcPermissionView.get('app_').id,
          createBoolPermission(
              PermissionType.kLocation, /*value=*/ false,
              /*is_managed=*/ false));
      await flushTasks();

      description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertEquals('Denied', description.textContent!.trim());
    });

    test('Tri-state permission display', async () => {
      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');

      fakeHandler.setPermission(
          arcPermissionView.get('app_').id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kAllow,
              /*is_managed=*/ false));
      await flushTasks();

      let description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertEquals('Allowed', description.textContent!.trim());

      fakeHandler.setPermission(
          arcPermissionView.get('app_').id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kAsk,
              /*is_managed=*/ false));
      await flushTasks();

      description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertEquals('Ask every time', description.textContent!.trim());

      fakeHandler.setPermission(
          arcPermissionView.get('app_').id,
          createTriStatePermission(
              PermissionType.kLocation, /*value=*/ TriState.kBlock,
              /*is_managed=*/ false));
      await flushTasks();

      description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertEquals('Denied', description.textContent!.trim());
    });


    test('Permission display with detail', async () => {
      const permission = createBoolPermission(
          PermissionType.kLocation, /*value=*/ true,
          /*is_managed=*/ false);
      permission.details = 'While in use';

      fakeHandler.setPermission(arcPermissionView.get('app_').id, permission);
      await flushTasks();

      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');

      let description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertTrue(description.textContent!.includes('While in use'));

      permission.value.boolValue = false;

      fakeHandler.setPermission(arcPermissionView.get('app_').id, permission);
      await flushTasks();

      description = locationItem.shadowRoot!.querySelector('#description');
      assertTrue(!!description);
      assertFalse(description.textContent!.includes('While in use'));
    });

    test('Managed permission has policy indicator', async () => {
      const permission = createBoolPermission(
          PermissionType.kLocation, /*value=*/ true,
          /*is_managed=*/ true);

      fakeHandler.setPermission(arcPermissionView.get('app_').id, permission);
      await flushTasks();

      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');
      assertTrue(
          !!locationItem.shadowRoot!.querySelector('cr-policy-indicator'));
    });

    test('Unmanaged permission has no policy indicator', async () => {
      const permission = createBoolPermission(
          PermissionType.kLocation, /*value=*/ true,
          /*is_managed=*/ false);

      fakeHandler.setPermission(arcPermissionView.get('app_').id, permission);
      await flushTasks();

      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');
      assertFalse(
          !!locationItem.shadowRoot!.querySelector('cr-policy-indicator'));
    });
  });
});
