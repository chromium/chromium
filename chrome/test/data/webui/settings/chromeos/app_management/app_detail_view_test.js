// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, updateArcSupported, FakePageHandler, ArcPermissionType, updateSelectedAppId, getPermissionValueBool, PageType, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden, getPermissionItemByType, getPermissionCrToggleByType} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-app-detail-view>', () => {
  let appDetailView;
  let fakeHandler;
  let arcApp;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateArcSupported(true));

    // Create an ARC app.
    const arcOptions = {type: apps.mojom.AppType.kArc};

    // Add an app, and make it the currently selected app.
    arcApp = await fakeHandler.addApp('app1_id', arcOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(arcApp.id));

    appDetailView = document.createElement('app-management-app-detail-view');

    replaceBody(appDetailView);
    await fakeHandler.flushPipesForTesting();
  });

  test('Change selected app', async () => {
    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        appDetailView.app_.id);
    assertEquals(arcApp.id, appDetailView.app_.id);
    assertTrue(!!appDetailView.$$('app-management-arc-detail-view'));
    assertFalse(!!appDetailView.$$('app-management-pwa-detail-view'));
    const pwaOptions = {type: apps.mojom.AppType.kWeb};
    // Add an second pwa app, and make it the currently selected app.
    const pwaApp = await fakeHandler.addApp('app2_id', pwaOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(pwaApp.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        appDetailView.app_.id);
    assertEquals(pwaApp.id, appDetailView.app_.id);
    assertFalse(!!appDetailView.$$('app-management-arc-detail-view'));
    assertTrue(!!appDetailView.$$('app-management-pwa-detail-view'));
  });
});
