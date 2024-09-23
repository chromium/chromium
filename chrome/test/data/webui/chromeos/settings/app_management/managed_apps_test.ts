// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementPwaDetailViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType, InstallReason, Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from './fake_page_handler.js';
import {getPermissionToggleByType, replaceBody, replaceStore, setupFakeHandler} from './test_util.js';

suite('<app-management-managed-apps>', () => {
  let appDetailView: AppManagementPwaDetailViewElement;
  let fakeHandler: FakePageHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create a Web app which is installed and pinned by policy
    // and has location set to on and camera set to off by policy.
    const permissionOptions: Partial<Record<PermissionType, Permission>> = {};
    permissionOptions[PermissionType.kLocation] = createTriStatePermission(
        PermissionType.kLocation, TriState.kAllow, /*isManaged*/ true);
    permissionOptions[PermissionType.kCamera] = createTriStatePermission(
        PermissionType.kCamera, TriState.kBlock, /*isManaged*/ true);
    const policyAppOptions = {
      type: AppType.kWeb,
      isPinned: true,
      isPolicyPinned: true,
      installReason: InstallReason.kPolicy,
      permissions: FakePageHandler.createWebPermissions(permissionOptions),
    };
    const app = await fakeHandler.addApp('', policyAppOptions);
    // Select created app.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    appDetailView = document.createElement('app-management-pwa-detail-view');
    replaceBody(appDetailView);
    await flushTasks();
  });

  teardown(() => {
    appDetailView.remove();
  });

  // TODO(crbug.com/40642807): rewrite test.
  test.skip('Uninstall button affected by policy', () => {
    const element = appDetailView.shadowRoot!.querySelector(
        'app-management-detail-view-header');
    assertTrue(!!element);
    const uninstallWrapper =
        element.shadowRoot!.querySelector('#uninstall-wrapper');
    assertTrue(!!uninstallWrapper);
    assertTrue(!!uninstallWrapper.querySelector('#policy-indicator'));
  });

  test('Permission toggles affected by policy', () => {
    function checkToggle(
        permissionType: PermissionTypeIndex, policyAffected: boolean) {
      const permissionToggle =
          getPermissionToggleByType(appDetailView, permissionType);
      const toggle = permissionToggle.shadowRoot!.querySelector('cr-toggle');
      assertTrue(!!toggle);
      assertEquals(policyAffected, toggle.disabled);
      assertEquals(
          policyAffected,
          !!permissionToggle.shadowRoot!.querySelector('#policyIndicator'));
    }
    checkToggle('kNotifications', false);
    checkToggle('kLocation', true);
    checkToggle('kCamera', true);
    checkToggle('kMicrophone', false);
  });

  test('Pin to shelf toggle effected by policy', () => {
    const pinToShelfSetting =
        appDetailView.shadowRoot!.querySelector('#pinToShelfSetting');
    assertTrue(!!pinToShelfSetting);
    const element = pinToShelfSetting.shadowRoot!.querySelector(
        'app-management-toggle-row');
    assertTrue(!!element);
    assertTrue(!!element.shadowRoot!.querySelector('#policyIndicator'));
    const toggle = element.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
  });
});
