// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-manageemnt-permission-item. */
import 'chrome://os-settings/lazy_load.js';

import {AppManagementPermissionItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, CrButtonElement, GeolocationAccessLevel, LocalizedLinkElement, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App, Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {fakeComponentBrowserProxy, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';
import {clearBody} from '../../utils.js';

type PermissionMap = Partial<Record<PermissionType, Permission>>;
suite('AppManagementPermissionItemTest', function() {
  let permissionItem: AppManagementPermissionItemElement;
  let fakeHandler: FakePageHandler;
  const app_id: string = 'app_id';
  const permissionType: PermissionTypeIndex = 'kLocation';

  function createPermissionItem(): void {
    clearBody();
    permissionItem = document.createElement('app-management-permission-item');
    permissionItem.app = getApp();
    permissionItem.permissionType = permissionType;
    permissionItem.prefs = {
      'ash': {
        'user': {
          'camera_allowed': {
            value: true,
          },
          'microphone_allowed': {
            value: true,
          },
          'geolocation_access_level': {
            value: GeolocationAccessLevel.ALLOWED,
          },
        },
      },
    };
    document.body.appendChild(permissionItem);
    flush();
  }

  setup(async function() {
    const permissions: PermissionMap = {};
    permissions[PermissionType[permissionType]] = createTriStatePermission(
        PermissionType[permissionType], TriState.kAsk, false);
    fakeHandler = setupFakeHandler();
    replaceStore();
    await fakeHandler.addApp(app_id, {permissions: permissions});
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app_id));

    createPermissionItem();
  });

  // Fetches the app state from the `AppManagementStore`.
  function getApp(): App {
    const app = AppManagementStore.getInstance().data.apps[app_id];
    assertTrue(!!app);
    return app;
  }

  test('Toggle permission', async function() {
    assertFalse(getPermissionValueBool(
        permissionItem.app, permissionItem.permissionType));

    permissionItem.click();

    const data = await fakeHandler.whenCalled('setPermission');
    assertEquals(data[1].value.tristateValue, TriState.kAllow);

    assertTrue(!!fakeComponentBrowserProxy);
    const metricData =
        await fakeComponentBrowserProxy.whenCalled('recordEnumerationValue');
    assertEquals(metricData[1], AppManagementUserAction.LOCATION_TURNED_ON);
  });

  test('Permission item has no description', async function() {
    assertNull(permissionItem.shadowRoot!.querySelector<HTMLElement>(
        '#permissionDescription'));
  });

  suite('Permission item with description', () => {
    setup(() => {
      loadTimeData.overrideValues({
        'privacyHubAppPermissionsV2Enabled': true,
        'privacyHubLocationAccessControlEnabled': true,
      });

      createPermissionItem();
    });

    teardown(() => {
      loadTimeData.overrideValues({
        'privacyHubAppPermissionsV2Enabled': false,
        'privacyHubLocationAccessControlEnabled': false,
      });
    });

    function getPermissionDescriptionString(): string {
      return permissionItem.shadowRoot!
          .querySelector<LocalizedLinkElement>(
              '#permissionDescription')!.localizedString.toString();
    }

    async function togglePermission(): Promise<void> {
      permissionItem.click();
      await flushTasks();
      permissionItem.set('app', getApp());
    }

    test('Toggle permission', async () => {
      assertEquals(
          loadTimeData.getString('appManagementPermissionAsk'),
          getPermissionDescriptionString());

      await togglePermission();

      assertEquals(
          loadTimeData.getString('appManagementPermissionAllowed'),
          getPermissionDescriptionString());

      await togglePermission();

      assertEquals(
          loadTimeData.getString('appManagementPermissionDenied'),
          getPermissionDescriptionString());
    });

    test('Turn on sensor system access button displayed', async () => {
      assertEquals(
          loadTimeData.getString('appManagementPermissionAsk'),
          getPermissionDescriptionString());

      await togglePermission();

      assertEquals(
          loadTimeData.getString('appManagementPermissionAllowed'),
          getPermissionDescriptionString());

      permissionItem.set(
          'prefs.ash.user.geolocation_access_level.value',
          GeolocationAccessLevel.DISALLOWED);

      assertEquals(
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnLocationAccessButton'),
          getPermissionDescriptionString());

      permissionItem.set(
          'prefs.ash.user.geolocation_access_level.value',
          GeolocationAccessLevel.ALLOWED);

      assertEquals(
          loadTimeData.getString('appManagementPermissionAllowed'),
          getPermissionDescriptionString());
    });

    function getDialogElement(): HTMLElement|null {
      return permissionItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    }

    async function openDialog(): Promise<void> {
      const permissionDescription =
          permissionItem.shadowRoot!.querySelector<LocalizedLinkElement>(
              '#permissionDescription');
      assertTrue(!!permissionDescription);
      const link = permissionDescription.shadowRoot!.querySelector('a');
      assertTrue(!!link);
      link.click();
      await waitAfterNextRender(permissionItem);
    }

    test('Open dialog and close using cancel button', async () => {
      await togglePermission();

      permissionItem.set(
          'prefs.ash.user.geolocation_access_level.value',
          GeolocationAccessLevel.DISALLOWED);

      assertEquals(
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnLocationAccessButton'),
          getPermissionDescriptionString());

      // Dialog not visible initially.
      assertNull(getDialogElement());

      await openDialog();

      // Dialog is visible.
      assertTrue(!!getDialogElement());

      // Close dialog.
      const cancelButton =
          getDialogElement()!.shadowRoot!.querySelector<CrButtonElement>(
              '#cancelButton');
      assertTrue(!!cancelButton);
      cancelButton.click();
      await waitAfterNextRender(permissionItem);

      // Dialog not visible anymore.
      assertNull(getDialogElement());
    });

    test('Open dialog and turn on sensor access', async () => {
      await togglePermission();

      permissionItem.set(
          'prefs.ash.user.geolocation_access_level.value',
          GeolocationAccessLevel.DISALLOWED);

      assertEquals(
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnLocationAccessButton'),
          getPermissionDescriptionString());

      // Dialog not visible initially.
      assertNull(getDialogElement());

      await openDialog();

      // Dialog is visible.
      assertTrue(!!getDialogElement());

      // Turn on system sensor access.
      const confirmButton =
          getDialogElement()!.shadowRoot!.querySelector<CrButtonElement>(
              '#confirmButton');
      assertTrue(!!confirmButton);
      confirmButton.click();
      await waitAfterNextRender(permissionItem);

      // Dialog not visible anymore.
      assertNull(getDialogElement());
      // Sensor access is turned ON.
      assertEquals(
          GeolocationAccessLevel.ALLOWED,
          permissionItem.prefs.ash.user.geolocation_access_level.value);
    });
  });
});
