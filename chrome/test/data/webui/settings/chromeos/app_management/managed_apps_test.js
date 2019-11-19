// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-managed-apps>', () => {
  let appDetailView;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create a Web app which is installed and pinned by policy
    // and has location set to on and camera set to off by policy.
    const permissionOptions = {};
    permissionOptions[PwaPermissionType.GEOLOCATION] = {
      permissionValue: TriState.kAllow,
      isManaged: true,
    };
    permissionOptions[PwaPermissionType.MEDIASTREAM_CAMERA] = {
      permissionValue: TriState.kBlock,
      isManaged: true
    };
    const policyAppOptions = {
      type: apps.mojom.AppType.kWeb,
      isPinned: apps.mojom.OptionalBool.kTrue,
      isPolicyPinned: apps.mojom.OptionalBool.kTrue,
      installSource: apps.mojom.InstallSource.kPolicy,
      permissions: app_management.FakePageHandler.createWebPermissions(
          permissionOptions),
    };
    const app = await fakeHandler.addApp(null, policyAppOptions);
    // Select created app.
    app_management.Store.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));
    appDetailView =
        document.createElement('app-management-pwa-permission-view');
    replaceBody(appDetailView);
    await test_util.flushTasks();
  });

  // TODO(crbug.com/999412): rewrite test.
  test.skip('Uninstall button affected by policy', () => {
    const uninstallWrapper =
        appDetailView.$$('app-management-permission-view-header')
            .$$('#uninstall-wrapper');
    expectTrue(!!uninstallWrapper.querySelector('#policy-indicator'));
  });

  test('Permission toggles affected by policy', () => {
    function checkToggle(permissionType, policyAffected) {
      const permissionToggle =
          getPermissionToggleByType(appDetailView, permissionType);
      expectTrue(permissionToggle.$$('cr-toggle').disabled === policyAffected);
      expectTrue(
          !!permissionToggle.root.querySelector('#policyIndicator') ===
          policyAffected);
    }
    checkToggle('NOTIFICATIONS', false);
    checkToggle('GEOLOCATION', true);
    checkToggle('MEDIASTREAM_CAMERA', true);
    checkToggle('MEDIASTREAM_MIC', false);
  });

  test('Pin to shelf toggle effected by policy', () => {
    const pinToShelfSetting = appDetailView.$$('#pin-to-shelf-setting')
                                  .$$('app-management-toggle-row');
    expectTrue(!!pinToShelfSetting.root.querySelector('#policyIndicator'));
    expectTrue(pinToShelfSetting.$$('cr-toggle').disabled);
  });
});
