// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementFileHandlingItemElement, AppManagementStore, AppManagementToggleRowElement, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from './fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from './test_util.js';

suite('<app-management-file-handling-item>', () => {
  let fileHandlingItem: AppManagementFileHandlingItemElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
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
  test('PWA - basic file handling test', async () => {
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

    const toggleRow =
        fileHandlingItem.shadowRoot!
            .querySelector<AppManagementToggleRowElement>('#toggle-row');
    assertTrue(!!toggleRow);
    assertFalse(toggleRow.isChecked());
  });
});
