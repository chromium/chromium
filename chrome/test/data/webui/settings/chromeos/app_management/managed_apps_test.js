// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {PermissionType, TriState, FakePageHandler, AppManagementStore, updateSelectedAppId, createTriStatePermission} from 'chrome://os-settings/chromeos/os_settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {setupFakeHandler, replaceStore, replaceBody, getPermissionToggleByType} from './test_util.js';
import {AppType, InstallReason, OptionalBool} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-managed-apps>', () => {
  let appDetailView;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create a Web app which is installed and pinned by policy
    // and has location set to on and camera set to off by policy.
    const permissionOptions = {};
    permissionOptions[PermissionType.kLocation] = createTriStatePermission(
        PermissionType.kLocation, TriState.kAllow, /*isManaged*/ true);
    permissionOptions[PermissionType.kCamera] = createTriStatePermission(
        PermissionType.kCamera, TriState.kBlock, /*isManaged*/ true);
    const policyAppOptions = {
      type: AppType.kWeb,
      isPinned: OptionalBool.kTrue,
      isPolicyPinned: OptionalBool.kTrue,
      installReason: InstallReason.kPolicy,
      permissions: FakePageHandler.createWebPermissions(permissionOptions),
    };
    const app = await fakeHandler.addApp(null, policyAppOptions);
    // Select created app.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    appDetailView = document.createElement('app-management-pwa-detail-view');
    replaceBody(appDetailView);
    await flushTasks();
  });

  // TODO(crbug.com/999412): rewrite test.
  test.skip('Uninstall button affected by policy', () => {
    const uninstallWrapper =
        appDetailView.shadowRoot
            .querySelector('app-management-detail-view-header')
            .shadowRoot.querySelector('#uninstall-wrapper');
    assertTrue(!!uninstallWrapper.querySelector('#policy-indicator'));
  });

  test('Permission toggles affected by policy', () => {
    function checkToggle(permissionType, policyAffected) {
      const permissionToggle =
          getPermissionToggleByType(appDetailView, permissionType);
      assertTrue(
          permissionToggle.shadowRoot.querySelector('cr-toggle').disabled ===
          policyAffected);
      assertTrue(
          !!permissionToggle.root.querySelector('#policyIndicator') ===
          policyAffected);
    }
    checkToggle('kNotifications', false);
    checkToggle('kLocation', true);
    checkToggle('kCamera', true);
    checkToggle('kMicrophone', false);
  });

  test('Pin to shelf toggle effected by policy', () => {
    const pinToShelfSetting =
        appDetailView.shadowRoot.querySelector('#pinToShelfSetting')
            .shadowRoot.querySelector('app-management-toggle-row');
    assertTrue(!!pinToShelfSetting.root.querySelector('#policyIndicator'));
    assertTrue(
        pinToShelfSetting.shadowRoot.querySelector('cr-toggle').disabled);
  });
});
