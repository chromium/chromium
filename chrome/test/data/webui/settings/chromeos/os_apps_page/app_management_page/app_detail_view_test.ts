// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementAppDetailViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-app-detail-view>', () => {
  let appDetailView: AppManagementAppDetailViewElement;
  let fakeHandler: FakePageHandler;
  let arcApp: App;

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
        appDetailView.get('app_').id);
    assertEquals(arcApp.id, appDetailView.get('app_').id);
    assertTrue(!!appDetailView.shadowRoot!.querySelector(
        'app-management-arc-detail-view'));
    assertNull(appDetailView.shadowRoot!.querySelector(
        'app-management-pwa-detail-view'));
    const pwaOptions = {type: AppType.kWeb};
    // Add an second pwa app, and make it the currently selected app.
    const pwaApp = await fakeHandler.addApp('app2_id', pwaOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(pwaApp.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        appDetailView.get('app_').id);
    assertEquals(pwaApp.id, appDetailView.get('app_').id);
    assertNull(appDetailView.shadowRoot!.querySelector(
        'app-management-arc-detail-view'));
    assertTrue(!!appDetailView.shadowRoot!.querySelector(
        'app-management-pwa-detail-view'));
  });
});
