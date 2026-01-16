// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {appPermissionHandlerMojom, CrIconButtonElement, CrToggleElement, SettingsPrivacyHubAppPermissionRow} from 'chrome://os-settings/os_settings.js';
import {setAppPermissionProviderForTesting} from 'chrome://os-settings/os_settings.js';
import type {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createTriStatePermission, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';

type App = appPermissionHandlerMojom.App;
type PermissionMap = Partial<Record<PermissionType, Permission>>;

suite('<settings-privacy-hub-app-permission-row>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let testRow: SettingsPrivacyHubAppPermissionRow;
  let app: App;
  const permissionType: PermissionTypeIndex = 'kMicrophone';

  setup(() => {
    loadTimeData.overrideValues({
      isArcReadOnlyPermissionsEnabled: false,
    });

    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);

    testRow = document.createElement('settings-privacy-hub-app-permission-row');
    testRow.permissionType = permissionType;
    app = {
      id: 'test_app_id',
      name: 'test_app_name',
      type: AppType.kWeb,
      permissions: {},
    };
    app.permissions[PermissionType[permissionType]] = createTriStatePermission(
        PermissionType[permissionType], TriState.kAsk, /*is_managed=*/ false);
    testRow.app = app;
    document.body.appendChild(testRow);
    flush();
  });

  teardown(() => {
    testRow.remove();
  });

  async function changePermissionValue(value: TriState): Promise<void> {
    const permissions: PermissionMap = {};
    permissions[PermissionType[permissionType]] = createTriStatePermission(
        PermissionType[permissionType], value, /*is_managed=*/ false);
    testRow.set('app.permissions', permissions);
    await waitAfterNextRender(testRow);
  }

  function getAppName(): string {
    return testRow.shadowRoot!.querySelector('#appName')!.textContent.trim();
  }

  function getPermissionText(): string {
    return testRow.shadowRoot!.querySelector(
                                  '#permissionText')!.textContent.trim();
  }

  function getPermissionToggle(): CrToggleElement {
    return testRow.shadowRoot!.querySelector<CrToggleElement>(
        '#permissionToggle')!;
  }

  test('Displays the app data appropriately', () => {
    assertEquals('test_app_name', getAppName());
    assertEquals(
        testRow.i18n('appManagementPermissionAsk'), getPermissionText());
    assertFalse(getPermissionToggle().checked);
  });

  test('Changing permission changes the subtext and toggle', async () => {
    const triStateDescription:
        {[key in TriState]: {text: string, isEnabled: boolean}} = {
          [TriState.kAllow]: {
            text: testRow.i18n('appManagementPermissionAllowed'),
            isEnabled: true,
          },
          [TriState.kAsk]: {
            text: testRow.i18n('appManagementPermissionAsk'),
            isEnabled: false,
          },
          [TriState.kBlock]: {
            text: testRow.i18n('appManagementPermissionDenied'),
            isEnabled: false,
          },
        };

    const permissionValues = [
      TriState.kBlock,
      TriState.kAllow,
      TriState.kAsk,
      TriState.kAllow,
      TriState.kBlock,
      TriState.kAsk,
    ];

    for (let i = 0; i < permissionValues.length; ++i) {
      const value = permissionValues[i]!;
      await changePermissionValue(value);
      assertEquals(triStateDescription[value].text, getPermissionText());
      assertEquals(
          triStateDescription[value].isEnabled, getPermissionToggle().checked);
    }
  });

  test('Type of permission changed is correct', async () => {
    assertEquals(
        PermissionType.kUnknown,
        fakeHandler.getLastUpdatedPermission().permissionType);

    testRow.click();
    await fakeHandler.whenCalled('setPermission');

    const updatedPermission = fakeHandler.getLastUpdatedPermission();
    assertEquals(
        permissionType, PermissionType[updatedPermission.permissionType]);
    assertTrue(isTriStateValue(updatedPermission.value));
    assertEquals(TriState.kAllow, updatedPermission.value.tristateValue);
  });

  function getLastUpdatedPermissionValue(): TriState|undefined {
    return fakeHandler.getLastUpdatedPermission().value.tristateValue;
  }

  test('Clicking on the toggle button triggers permission change', async () => {
    const lastUpdatedPermissionValue = getLastUpdatedPermissionValue();

    getPermissionToggle().click();
    await fakeHandler.whenCalled('setPermission');

    assertNotEquals(
        lastUpdatedPermissionValue, getLastUpdatedPermissionValue());
  });

  test('Clicking anywhere on the row triggers permission change', async () => {
    const lastUpdatedPermissionValue = getLastUpdatedPermissionValue();

    testRow.click();
    await fakeHandler.whenCalled('setPermission');

    assertNotEquals(
        lastUpdatedPermissionValue, getLastUpdatedPermissionValue());
  });

  test('Toggle button reacts to Enter and Space keyboard events', async () => {
    const keyBoardEvents = [
      {
        event: new KeyboardEvent('keydown', {key: 'Enter'}),
        shouldTogglePermission: true,
      },
      {
        event: new KeyboardEvent('keyup', {key: 'Enter'}),
        shouldTogglePermission: false,
      },
      {
        event: new KeyboardEvent('keydown', {key: ' '}),
        shouldTogglePermission: false,
      },
      {
        event: new KeyboardEvent('keyup', {key: ' '}),
        shouldTogglePermission: true,
      },
    ];

    for (const e of keyBoardEvents) {
      const lastUpdatedPermissionValue = getLastUpdatedPermissionValue();

      getPermissionToggle().dispatchEvent(e.event);
      await flushTasks();
      if (e.shouldTogglePermission) {
        assertNotEquals(
            lastUpdatedPermissionValue, getLastUpdatedPermissionValue());
      } else {
        assertEquals(
            lastUpdatedPermissionValue, getLastUpdatedPermissionValue());
      }
    }
  });

  function isPermissionManaged(): boolean {
    const permission = app.permissions[PermissionType[permissionType]];
    assertTrue(!!permission);
    return permission.isManaged;
  }

  test('Managed icon displayed when permission is managed', () => {
    assertFalse(isPermissionManaged());
    assertNull(testRow.shadowRoot!.querySelector('cr-policy-indicator'));
    assertFalse(getPermissionToggle().disabled);

    // Toggle managed state.
    testRow.set(
        'app.permissions.' + PermissionType[permissionType] + '.isManaged',
        true);
    flush();

    assertTrue(isPermissionManaged());
    assertTrue(!!testRow.shadowRoot!.querySelector('cr-policy-indicator'));
    assertTrue(getPermissionToggle().disabled);
  });

  test('Clicking on the row is no-op when permission is managed', async () => {
    assertFalse(isPermissionManaged());

    // Toggle managed state.
    testRow.set(
        'app.permissions.' + PermissionType[permissionType] + '.isManaged',
        true);
    flush();

    assertTrue(isPermissionManaged());
    const lastUpdatedPermissionValue = getLastUpdatedPermissionValue();

    testRow.click();
    await flushTasks();

    assertEquals(lastUpdatedPermissionValue, getLastUpdatedPermissionValue());
  });

  function getAndroidSettingsLinkButton(): CrIconButtonElement|null {
    return testRow.shadowRoot!.querySelector('cr-icon-button');
  }

  test('Link to android settings displayed', () => {
    assertFalse(isVisible(getAndroidSettingsLinkButton()));

    loadTimeData.overrideValues({
      isArcReadOnlyPermissionsEnabled: true,
    });
    testRow.set('app.type', AppType.kArc);
    flush();

    assertTrue(isVisible(getAndroidSettingsLinkButton()));
  });

  test(
      'Clicking the android settings link opens android settings', async () => {
        loadTimeData.overrideValues({
          isArcReadOnlyPermissionsEnabled: true,
        });
        testRow.set('app.type', AppType.kArc);
        flush();

        assertEquals(0, fakeHandler.getNativeSettingsOpenedCount());

        const linkButton = getAndroidSettingsLinkButton();
        assertTrue(!!linkButton);
        linkButton.click();
        await fakeHandler.whenCalled('openNativeSettings');

        assertEquals(1, fakeHandler.getNativeSettingsOpenedCount());
      });

  test('Clicking anywhere on the row opens android settings', async () => {
    loadTimeData.overrideValues({
      isArcReadOnlyPermissionsEnabled: true,
    });
    testRow.set('app.type', AppType.kArc);
    flush();

    assertEquals(0, fakeHandler.getNativeSettingsOpenedCount());

    testRow.click();
    await fakeHandler.whenCalled('openNativeSettings');

    assertEquals(1, fakeHandler.getNativeSettingsOpenedCount());
  });
});
