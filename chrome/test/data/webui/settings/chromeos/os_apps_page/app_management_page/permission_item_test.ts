// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-manageemnt-permission-item. */
import 'chrome://os-settings/lazy_load.js';

import {AppManagementPermissionItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {fakeComponentBrowserProxy, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

type PermissionMap = Partial<Record<PermissionType, Permission>>;
suite('AppManagementPermissionItemTest', function() {
  let permissionItem: AppManagementPermissionItemElement;
  let fakeHandler: FakePageHandler;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const permissions: PermissionMap = {};
    permissions[PermissionType.kLocation] = createTriStatePermission(
        PermissionType.kLocation, TriState.kAsk, false);
    fakeHandler = setupFakeHandler();
    replaceStore();
    const app = await fakeHandler.addApp('app', {permissions: permissions});
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    permissionItem = document.createElement('app-management-permission-item');
    permissionItem.app = app;
    permissionItem.permissionType = 'kLocation';
    document.body.appendChild(permissionItem);
    await waitAfterNextRender(permissionItem);
  });

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
});
