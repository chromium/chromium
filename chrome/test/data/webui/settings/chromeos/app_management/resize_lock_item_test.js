// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/os_settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {setupFakeHandler, replaceBody, isHidden} from './test_util.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-resize-lock-item>', () => {
  let resizeLockItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    resizeLockItem = document.createElement('app-management-resize-lock-item');

    replaceBody(resizeLockItem);
    flushTasks();
  });

  test('Resize lock setting visibility', async () => {
    // Add an arc app with the default options, and make it the currently
    // selected app.
    const defaultArcOptions = {
      type: AppType.kArc,
      permissions: {},
    };
    const defaultArcApp = await fakeHandler.addApp('app', defaultArcOptions);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = defaultArcApp;
    // The resize lock setting is hidden by default.
    assertTrue(isHidden(resizeLockItem));

    // Enable resize lock, but it's still hidden.
    const arcOptionsWithResizeLocked = {
      type: AppType.kArc,
      resizeLocked: true,
    };
    const appWithResizeLocked =
        await fakeHandler.addApp(null, arcOptionsWithResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Disable resize lock again, and it's still hidden.
    const arcOptionsWithoutResizeLocked = {
      type: AppType.kArc,
      resizeLocked: false,
    };
    const appWithoutResizeLocked =
        await fakeHandler.addApp(null, arcOptionsWithoutResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithoutResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| to false shows the setting.
    const arcOptionsWithHideResizeLockedFalse = {
      type: AppType.kArc,
      hideResizeLocked: false,
    };
    const appWithHideResizeLockedFalse =
        await fakeHandler.addApp(null, arcOptionsWithHideResizeLockedFalse);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedFalse;
    assertFalse(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| back to true hides the setting.
    const arcOptionsWithHideResizeLockedTrue = {
      type: AppType.kArc,
      hideResizeLocked: true,
    };
    const appWithHideResizeLockedTrue =
        await fakeHandler.addApp(null, arcOptionsWithHideResizeLockedTrue);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedTrue;
    assertTrue(isHidden(resizeLockItem));
  });
});
