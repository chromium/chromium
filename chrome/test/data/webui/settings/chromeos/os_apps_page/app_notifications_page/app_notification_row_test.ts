// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppNotificationRowElement} from 'chrome://os-settings/lazy_load.js';
import {appNotificationHandlerMojom, CrToggleElement, setAppNotificationProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermission, getBoolPermissionValue, isBoolValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

import {FakeAppNotificationHandler} from './fake_app_notification_handler.js';

const {Readiness} = appNotificationHandlerMojom;

type App = appNotificationHandlerMojom.App;
type ReadinessType = appNotificationHandlerMojom.Readiness;

suite('<app-notification-row>', () => {
  let row: AppNotificationRowElement;
  let mojoApi: FakeAppNotificationHandler;

  function createApp(
      id: string, title: string, notificationPermission: Permission,
      readiness: ReadinessType = Readiness.kReady): App {
    return {
      id,
      title,
      notificationPermission,
      readiness,
    };
  }

  async function createRowForApp(app: App): Promise<void> {
    clearBody();
    row = document.createElement('app-notification-row');
    row.app = app;
    document.body.appendChild(row);
    await flushTasks();
  }

  function getRowToggle(): CrToggleElement {
    const toggle = row.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!toggle);
    return toggle;
  }

  suiteSetup(() => {
    mojoApi = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi);
  });

  teardown(() => {
    mojoApi.resetForTest();
  });

  test('Row reflects managed app state', async () => {
    const permission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ false, /*is_managed=*/ true);
    const app = createApp(/*id=*/ 'app-id', /*title=*/ 'App', permission);
    await createRowForApp(app);

    assertFalse(row.shadowRoot!.querySelector('#container')!.hasAttribute(
        'actionable'));
    assertTrue(isVisible(row.shadowRoot!.querySelector('cr-policy-indicator')));
    assertTrue(getRowToggle().disabled);
  });

  test('Row reflects non-managed app state', async () => {
    const permission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ false, /*is_managed=*/ false);
    const app = createApp(/*id=*/ 'app-id', /*title=*/ 'App', permission);
    await createRowForApp(app);

    assertTrue(row.shadowRoot!.querySelector('#container')!.hasAttribute(
        'actionable'));
    assertFalse(
        isVisible(row.shadowRoot!.querySelector('cr-policy-indicator')));
    assertFalse(getRowToggle().disabled);
  });

  test('Toggle reflects current app notification state', async () => {
    const permission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ true, /*is_managed=*/ false);
    const app = createApp(/*id=*/ 'app-id', /*title=*/ 'App', permission);
    await createRowForApp(app);

    const toggle = getRowToggle();
    assertTrue(toggle.checked);

    const updatedPermission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ false, /*is_managed=*/ false);
    row.app = {...app, notificationPermission: updatedPermission};
    await flushTasks();
    assertFalse(toggle.checked);
  });

  test('Clicking the row updates the notification on/off', async () => {
    const appId = 'app-id';
    const permission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ true, /*is_managed=*/ false);
    const app = createApp(appId, /*title=*/ 'App', permission);
    await createRowForApp(app);

    assertTrue(getRowToggle().checked);

    row.click();
    await mojoApi.whenCalled('setNotificationPermission');

    assertEquals(appId, mojoApi.getLastUpdatedAppId());
    const lastUpdatedPermission = mojoApi.getLastUpdatedPermission();
    assertTrue(isBoolValue(lastUpdatedPermission.value));
    assertFalse(getBoolPermissionValue(lastUpdatedPermission.value));
  });

  test('Keypress on the toggle updates the notification on/off', async () => {
    const appId = 'app-id';
    const permission = createBoolPermission(
        /*permissionType=*/ 1,
        /*value=*/ true, /*is_managed=*/ false);
    const app = createApp(appId, /*title=*/ 'App', permission);
    await createRowForApp(app);

    const toggle = getRowToggle();
    assertTrue(toggle.checked);

    toggle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await mojoApi.whenCalled('setNotificationPermission');

    assertEquals(appId, mojoApi.getLastUpdatedAppId());
    const lastUpdatedPermission = mojoApi.getLastUpdatedPermission();
    assertTrue(isBoolValue(lastUpdatedPermission.value));
    assertFalse(getBoolPermissionValue(lastUpdatedPermission.value));
  });
});
