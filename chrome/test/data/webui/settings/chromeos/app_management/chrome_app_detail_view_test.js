// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, updateArcSupported, FakePageHandler, ArcPermissionType, updateSelectedAppId, getPermissionValueBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden, getPermissionItemByType, getPermissionCrToggleByType} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-chrome-app-detail-view>', () => {
  let chromeAppDetailView;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const chromeOptions = {
      type: apps.mojom.AppType.kExtension,
      permissions: {}
    };

    // Add an chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, chromeOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    chromeAppDetailView =
        document.createElement('app-management-chrome-app-detail-view');
    replaceBody(chromeAppDetailView);
    fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        app_management.AppManagementStore.getInstance().data.selectedAppId,
        chromeAppDetailView.app_.id);
    assertTrue(!!chromeAppDetailView.$$('app-management-pin-to-shelf-item'));
    assertTrue(!!chromeAppDetailView.$$('#more-settings'));
  });
});
