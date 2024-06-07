// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementArcDetailViewElement, MediaDevicesProxy} from 'chrome://os-settings/lazy_load.js';
import {AppManagementReadOnlyPermissionItemElement, AppManagementStore, CrButtonElement, CrToggleElement, GeolocationAccessLevel, LocalizedLinkElement, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermission, createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {addFakeSensor, getPermissionCrToggleByType, getPermissionItemByType, isHidden, isHiddenByDomIf, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';
import {FakeMediaDevices} from '../../fake_media_devices.js';

function getFakePrefs() {
  return {
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
}

// TODO(b/270728282) - remove "as" cast once getPermissionCrToggleByType()
// becomes a TS function.
suite('<app-management-arc-detail-view>', () => {
  let arcPermissionView: AppManagementArcDetailViewElement;
  let fakeHandler: FakePageHandler;
  let mediaDevices: FakeMediaDevices;

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
    arcPermissionView.prefs = getFakePrefs();
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

  function getPermissionDescriptionString(permissionItem: Element): string {
    return permissionItem.shadowRoot!
        .querySelector<LocalizedLinkElement>(
            '#permissionDescription')!.localizedString.toString();
  }

  async function setPermission(
      permissionType: PermissionType, value: TriState): Promise<void> {
    fakeHandler.setPermission(
        arcPermissionView.get('app_').id,
        createTriStatePermission(permissionType, value, /*is_managed=*/ false));
    await flushTasks();
  }

  suite('Read-only permissions', () => {
    setup(async () => {
      loadTimeData.overrideValues(
          {'appManagementArcReadOnlyPermissions': true});

      // Re-render with the new loadTimeData.
      arcPermissionView =
          document.createElement('app-management-arc-detail-view');
      arcPermissionView.prefs = getFakePrefs();
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

      assertEquals('Allowed', getPermissionDescriptionString(locationItem));

      // Simulate the permission being changed by the OS, and verify that the
      // description text updates.
      fakeHandler.setPermission(
          arcPermissionView.get('app_').id,
          createBoolPermission(
              PermissionType.kLocation, /*value=*/ false,
              /*is_managed=*/ false));
      await flushTasks();

      assertEquals('Denied', getPermissionDescriptionString(locationItem));
    });

    test('Tri-state permission display', async () => {
      const locationItem =
          getPermissionItemByType(arcPermissionView, 'kLocation');

      await setPermission(PermissionType.kLocation, TriState.kAllow);

      assertEquals('Allowed', getPermissionDescriptionString(locationItem));

      await setPermission(PermissionType.kLocation, TriState.kAsk);

      assertEquals(
          'Ask every time', getPermissionDescriptionString(locationItem));

      await setPermission(PermissionType.kLocation, TriState.kBlock);

      assertEquals('Denied', getPermissionDescriptionString(locationItem));
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

      assertTrue(getPermissionDescriptionString(locationItem)
                     .includes('While in use'));

      permission.value.boolValue = false;

      fakeHandler.setPermission(arcPermissionView.get('app_').id, permission);
      await flushTasks();

      assertFalse(getPermissionDescriptionString(locationItem)
                      .includes('While in use'));
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

  suite(
      'System wide sensor access control from read-only permission items',
      () => {
        setup(async () => {
          mediaDevices = new FakeMediaDevices();
          MediaDevicesProxy.setMediaDevicesForTesting(mediaDevices);

          loadTimeData.overrideValues({
            'appManagementArcReadOnlyPermissions': true,
            'privacyHubAppPermissionsV2Enabled': true,
            'privacyHubLocationAccessControlEnabled': true,
          });

          // Add an arc app with camera, location and microphone permission, and
          // make it the currently selected app.
          const arcOptions = {
            type: AppType.kArc,
            permissions: FakePageHandler.createArcPermissions([
              PermissionType.kCamera,
              PermissionType.kLocation,
              PermissionType.kMicrophone,
            ]),
          };
          const app = await fakeHandler.addApp('id', arcOptions);
          AppManagementStore.getInstance().dispatch(
              updateSelectedAppId(app.id));

          // Re-render with the new loadTimeData.
          arcPermissionView =
              document.createElement('app-management-arc-detail-view');
          arcPermissionView.prefs = getFakePrefs();
          replaceBody(arcPermissionView);
          await flushTasks();
        });

        teardown(() => {
          arcPermissionView.remove();
          loadTimeData.overrideValues({
            'appManagementArcReadOnlyPermissions': false,
            'privacyHubAppPermissionsV2Enabled': false,
            'privacyHubLocationAccessControlEnabled': false,
          });
        });

        function getDialogElement(permissionItem: HTMLElement): HTMLElement|
            null {
          return permissionItem.shadowRoot!.querySelector<HTMLElement>(
              '#dialog');
        }

        async function openDialog(permissionItem: HTMLElement): Promise<void> {
          const permissionDescription =
              permissionItem.shadowRoot!.querySelector<LocalizedLinkElement>(
                  '#permissionDescription');
          assertTrue(!!permissionDescription);
          const link = permissionDescription.shadowRoot!.querySelector('a');
          assertTrue(!!link);
          link.click();
          await flushTasks();
        }

        test('Open dialog and close using cancel button', async () => {
          const locationItem =
              arcPermissionView.shadowRoot!
                  .querySelector<AppManagementReadOnlyPermissionItemElement>(
                      '[permission-type=kLocation]');
          assertTrue(!!locationItem);

          await setPermission(PermissionType.kLocation, TriState.kAllow);

          assertEquals(
              loadTimeData.getString('appManagementPermissionAllowed'),
              getPermissionDescriptionString(locationItem));

          arcPermissionView.set(
              'prefs.ash.user.geolocation_access_level.value',
              GeolocationAccessLevel.DISALLOWED);

          assertEquals(
              loadTimeData.getString(
                  'permissionAllowedTextWithTurnOnLocationAccessButton'),
              getPermissionDescriptionString(locationItem));

          // Dialog not visible initially.
          assertNull(getDialogElement(locationItem));

          await openDialog(locationItem);

          // Dialog is visible.
          assertTrue(!!getDialogElement(locationItem));

          // Close dialog.
          const cancelButton =
              getDialogElement(locationItem)!.shadowRoot!
                  .querySelector<CrButtonElement>('#cancelButton');
          assertTrue(!!cancelButton);
          cancelButton.click();
          await waitAfterNextRender(locationItem);

          // Dialog not visible anymore.
          assertNull(getDialogElement(locationItem));
        });

        test('Open dialog and turn on sensor access', async () => {
          const locationItem =
              arcPermissionView.shadowRoot!
                  .querySelector<AppManagementReadOnlyPermissionItemElement>(
                      '[permission-type=kLocation]');
          assertTrue(!!locationItem);

          await setPermission(PermissionType.kLocation, TriState.kAllow);
          locationItem.set(
              'prefs.ash.user.geolocation_access_level.value',
              GeolocationAccessLevel.DISALLOWED);

          assertEquals(
              loadTimeData.getString(
                  'permissionAllowedTextWithTurnOnLocationAccessButton'),
              getPermissionDescriptionString(locationItem));

          // Dialog not visible initially.
          assertNull(getDialogElement(locationItem));

          await openDialog(locationItem);

          // Dialog is visible.
          assertTrue(!!getDialogElement(locationItem));

          // Turn on system sensor access.
          const confirmButton =
              getDialogElement(locationItem)!.shadowRoot!
                  .querySelector<CrButtonElement>('#confirmButton');
          assertTrue(!!confirmButton);
          confirmButton.click();
          await waitAfterNextRender(locationItem);

          // Sensor access is turned ON.
          assertNull(getDialogElement(locationItem));
          assertEquals(
              GeolocationAccessLevel.ALLOWED,
              locationItem.get('prefs.ash.user.geolocation_access_level')
                  .value);
        });

        test(
            'Permission description updated when no sensor connected',
            async () => {
              const checkPermissionDescription = async (
                  permissionType: PermissionTypeIndex,
                  expectedDescription: string) => {
                const permissionItem =
                    getPermissionItemByType(arcPermissionView, permissionType);

                await setPermission(
                    PermissionType[permissionType], TriState.kAsk);

                assertEquals(
                    loadTimeData.getString('appManagementPermissionAsk'),
                    getPermissionDescriptionString(permissionItem));

                await setPermission(
                    PermissionType[permissionType], TriState.kAllow);

                assertEquals(
                    expectedDescription,
                    getPermissionDescriptionString(permissionItem));

                await addFakeSensor(mediaDevices, permissionType);

                assertEquals(
                    loadTimeData.getString('appManagementPermissionAllowed'),
                    getPermissionDescriptionString(permissionItem));

                mediaDevices.popDevice();
                await flushTasks();

                assertEquals(
                    expectedDescription,
                    getPermissionDescriptionString(permissionItem));
              };

              await checkPermissionDescription(
                  'kCamera',
                  loadTimeData.getString(
                      'permissionAllowedButNoCameraConnectedText'));
              await checkPermissionDescription(
                  'kMicrophone',
                  loadTimeData.getString(
                      'permissionAllowedButNoMicrophoneConnectedText'));
            });

        test(
            'Permission description updated when microphone hw switch ON',
            async () => {
              const permissionItem =
                  getPermissionItemByType(arcPermissionView, 'kMicrophone');

              await setPermission(PermissionType.kMicrophone, TriState.kAllow);

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedButNoMicrophoneConnectedText'),
                  getPermissionDescriptionString(permissionItem));

              await addFakeSensor(mediaDevices, 'kMicrophone');

              assertEquals(
                  loadTimeData.getString('appManagementPermissionAllowed'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback('microphone-hardware-toggle-changed', true);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedButMicrophoneHwSwitchActiveText'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback(
                  'microphone-hardware-toggle-changed', false);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString('appManagementPermissionAllowed'),
                  getPermissionDescriptionString(permissionItem));
            });

        // Camera toggle button is force-disabled in CRD session.
        test(
            'Allow camera access button hidden if camera toggle force disabled',
            async () => {
              const permissionItem =
                  getPermissionItemByType(arcPermissionView, 'kCamera');

              await setPermission(PermissionType.kCamera, TriState.kAllow);
              arcPermissionView.set(
                  'prefs.ash.user.camera_allowed.value', false);
              await addFakeSensor(mediaDevices, 'kCamera');

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedTextWithTurnOnCameraAccessButton'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback('force-disable-camera-switch', true);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString('appManagementPermissionAllowed'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback('force-disable-camera-switch', false);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedTextWithTurnOnCameraAccessButton'),
                  getPermissionDescriptionString(permissionItem));
            });

        test(
            'Allow mic access button hidden if mic muted by security curtain',
            async () => {
              const permissionItem =
                  getPermissionItemByType(arcPermissionView, 'kMicrophone');

              await setPermission(PermissionType.kMicrophone, TriState.kAllow);
              arcPermissionView.set(
                  'prefs.ash.user.microphone_allowed.value', false);
              await addFakeSensor(mediaDevices, 'kMicrophone');

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedTextWithTurnOnMicrophoneAccessButton'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback(
                  'microphone-muted-by-security-curtain-changed', true);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString('appManagementPermissionAllowed'),
                  getPermissionDescriptionString(permissionItem));

              webUIListenerCallback(
                  'microphone-muted-by-security-curtain-changed', false);
              await waitAfterNextRender(arcPermissionView);

              assertEquals(
                  loadTimeData.getString(
                      'permissionAllowedTextWithTurnOnMicrophoneAccessButton'),
                  getPermissionDescriptionString(permissionItem));
            });
      });
});
