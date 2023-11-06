// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {appPermissionHandlerMojom, CrToggleElement, setAppPermissionProviderForTesting, SettingsPrivacyHubAppPermissionRow} from 'chrome://os-settings/os_settings.js';
import {Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createTriStatePermission, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';

type App = appPermissionHandlerMojom.App;
type PermissionMap = Partial<Record<PermissionType, Permission>>;

suite('<settings-privacy-hub-app-permission-row>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let testRow: SettingsPrivacyHubAppPermissionRow;
  let app: App;
  const permissionType: PermissionTypeIndex = 'kMicrophone';

  setup(() => {
    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);

    testRow = document.createElement('settings-privacy-hub-app-permission-row');
    testRow.permissionType = permissionType;
    app = {id: 'test_app_id', name: 'test_app_name', permissions: {}};
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
    return testRow.shadowRoot!.querySelector('#appName')!.textContent!.trim();
  }

  function getPermissionText(): string {
    return testRow.shadowRoot!.querySelector(
                                  '#permissionText')!.textContent!.trim();
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

  test('Clicking on the toggle triggers permission update', async () => {
    assertEquals(
        PermissionType.kUnknown,
        fakeHandler.getLastUpdatedPermission().permissionType);

    getPermissionToggle().click();
    await fakeHandler.whenCalled('setPermission');

    const updatedPermission = fakeHandler.getLastUpdatedPermission();
    assertEquals(
        permissionType, PermissionType[updatedPermission.permissionType]);
    assertTrue(isTriStateValue(updatedPermission.value));
    assertEquals(TriState.kAllow, updatedPermission.value.tristateValue);
  });
});
