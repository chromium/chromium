// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://test/test_util.js';

suite('<app-management-chrome-app-detail-view>', () => {
  let chromeAppDetailView;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const chromeOptions = {
      type: appManagement.mojom.AppType.kExtension,
      permissions: {}
    };

    // Add an chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, chromeOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    fakeHandler.flushPipesForTesting();
    await flushTasks();

    chromeAppDetailView =
        document.createElement('app-management-chrome-app-detail-view');
    replaceBody(chromeAppDetailView);
    fakeHandler.flushPipesForTesting();
    await flushTasks();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        chromeAppDetailView.app_.id);
    assertTrue(!!chromeAppDetailView.$$('app-management-pin-to-shelf-item'));
    assertTrue(!!chromeAppDetailView.$$('#more-settings'));
  });
});
