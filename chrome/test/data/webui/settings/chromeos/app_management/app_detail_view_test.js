// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody} from './test_util.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-app-detail-view>', () => {
  let appDetailView;
  let fakeHandler;
  let arcApp;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create an ARC app.
    const arcOptions = {type: AppType.kArc};

    // Add an app, and make it the currently selected app.
    arcApp = await fakeHandler.addApp('app1_id', arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(arcApp.id));

    appDetailView = document.createElement('app-management-app-detail-view');

    replaceBody(appDetailView);
    await fakeHandler.flushPipesForTesting();
  });

  test('Change selected app', async () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        appDetailView.app_.id);
    assertEquals(arcApp.id, appDetailView.app_.id);
    assertTrue(!!appDetailView.shadowRoot.querySelector(
        'app-management-arc-detail-view'));
    assertFalse(!!appDetailView.shadowRoot.querySelector(
        'app-management-pwa-detail-view'));
    const pwaOptions = {type: AppType.kWeb};
    // Add an second pwa app, and make it the currently selected app.
    const pwaApp = await fakeHandler.addApp('app2_id', pwaOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(pwaApp.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        appDetailView.app_.id);
    assertEquals(pwaApp.id, appDetailView.app_.id);
    assertFalse(!!appDetailView.shadowRoot.querySelector(
        'app-management-arc-detail-view'));
    assertTrue(!!appDetailView.shadowRoot.querySelector(
        'app-management-pwa-detail-view'));
  });
});
