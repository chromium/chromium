// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-file-handling-item>', () => {
  let fileHandlingItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    fileHandlingItem =
        document.createElement('app-management-file-handling-item');

    replaceBody(fileHandlingItem);
    flushTasks();
  });

  // Simple test that just verifies the file handling item is present and
  // doesn't throw errors. More comprehensive testing is in cross platform
  // app_management tests.
  test('PWA - basic file handling test', async function() {
    const pwaOptions = {
      type: AppType.kWeb,
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

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    fileHandlingItem.app = app;

    replaceBody(fileHandlingItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertFalse(
        fileHandlingItem.shadowRoot.querySelector('#toggle-row').isChecked());
  });
});
