// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore} from 'chrome://os-settings/chromeos/os_settings.js';
import {convertOptionalBoolToBool} from 'chrome://resources/cr_components/app_management/util.js';
import {setupFakeHandler, replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-pin-to-shelf-item>', () => {
  let pinToShelfItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    pinToShelfItem = document.createElement('app-management-pin-to-shelf-item');

    replaceBody(pinToShelfItem);
    flushTasks();
  });

  test('Toggle pin to shelf', async () => {
    const arcOptions = {
      type: AppType.kArc,
      permissions: {},
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', arcOptions);

    await fakeHandler.flushPipesForTesting();
    pinToShelfItem.app = app;
    assertFalse(convertOptionalBoolToBool(
        AppManagementStore.getInstance().data.apps[app.id].isPinned));

    pinToShelfItem.click();
    flushTasks();
    await fakeHandler.flushPipesForTesting();

    assertTrue(convertOptionalBoolToBool(
        AppManagementStore.getInstance().data.apps[app.id].isPinned));
  });
});
