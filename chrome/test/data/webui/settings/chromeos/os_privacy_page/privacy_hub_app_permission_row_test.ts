// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {appPermissionHandlerMojom, CrIconButtonElement, CrToggleElement, PrivacyHubSensorSubpageUserAction, setAppPermissionProviderForTesting, SettingsPrivacyHubAppPermissionRow} from 'chrome://os-settings/os_settings.js';
import {AppType, Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createTriStatePermission, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';
import {createFakeMetricsPrivate} from './privacy_hub_app_permission_test_util.js';

type App = appPermissionHandlerMojom.App;
type PermissionMap = Partial<Record<PermissionType, Permission>>;

suite('<settings-privacy-hub-app-permission-row>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let metrics: FakeMetricsPrivate;
  let testRow: SettingsPrivacyHubAppPermissionRow;
  let app: App;
  const permissionType: PermissionTypeIndex = 'kMicrophone';

  setup(() => {
    loadTimeData.overrideValues({
      isArcReadOnlyPermissionsEnabled: false,
    });

    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);

    metrics = createFakeMetricsPrivate();

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
        0,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.APP_PERMISSION_CHANGED));
    assertEquals(
        PermissionType.kUnknown,
        fakeHandler.getLastUpdatedPermission().permissionType);

    getPermissionToggle().click();
    await fakeHandler.whenCalled('setPermission');

    assertEquals(
        1,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.APP_PERMISSION_CHANGED));
    const updatedPermission = fakeHandler.getLastUpdatedPermission();
    assertEquals(
        permissionType, PermissionType[updatedPermission.permissionType]);
    assertTrue(isTriStateValue(updatedPermission.value));
    assertEquals(TriState.kAllow, updatedPermission.value.tristateValue);
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

  function getAndroidSettingsLinkButton(): CrIconButtonElement|null {
    return testRow.shadowRoot!.querySelector('cr-icon-button');
  }

  test('Link to android settings displayed', async () => {
    assertNull(getAndroidSettingsLinkButton());

    loadTimeData.overrideValues({
      isArcReadOnlyPermissionsEnabled: true,
    });
    testRow.set('app.type', AppType.kArc);
    flush();

    assertEquals(
        0,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.ANDROID_SETTINGS_LINK_CLICKED));

    const linkButton = getAndroidSettingsLinkButton();
    assertTrue(!!linkButton);
    linkButton.click();
    await fakeHandler.whenCalled('openNativeSettings');

    assertEquals(
        1,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.ANDROID_SETTINGS_LINK_CLICKED));
    assertEquals(1, fakeHandler.getNativeSettingsOpenedCount());
  });
});
