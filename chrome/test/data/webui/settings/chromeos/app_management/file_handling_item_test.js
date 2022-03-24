// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, updateSelectedAppId, addApp} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// clang-format on

'use strict';

suite('<app-management-file-handling-item>', () => {
  let fileHandlingItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    fileHandlingItem =
        document.createElement('app-management-file-handling-item');

    replaceBody(fileHandlingItem);
    test_util.flushTasks();
  });

  // Simple test that just verifies the file handling item is present and
  // doesn't throw errors. More comprehensive testing is in cross platform
  // app_management tests.
  test('PWA - basic file handling test', async function() {
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      fileHandlingState: {
        enabled: false,
        isManaged: false,
        userVisibleTypes: 'TXT',
        userVisibleTypesLabel: 'Supported type: TXT',
        learnMoreUrl: {url: 'https://google.com/'},
      },
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    fileHandlingItem.app = app;

    replaceBody(fileHandlingItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectFalse(
        fileHandlingItem.shadowRoot.querySelector('#toggle-row').isChecked());
  });
});
