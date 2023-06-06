// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementResizeLockItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {isHidden, replaceBody, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-resize-lock-item>', () => {
  let resizeLockItem: AppManagementResizeLockItemElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    resizeLockItem = document.createElement('app-management-resize-lock-item');

    replaceBody(resizeLockItem);
    flushTasks();
  });

  teardown(() => {
    resizeLockItem.remove();
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
        await fakeHandler.addApp('', arcOptionsWithResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Disable resize lock again, and it's still hidden.
    const arcOptionsWithoutResizeLocked = {
      type: AppType.kArc,
      resizeLocked: false,
    };
    const appWithoutResizeLocked =
        await fakeHandler.addApp('', arcOptionsWithoutResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithoutResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| to false shows the setting.
    const arcOptionsWithHideResizeLockedFalse = {
      type: AppType.kArc,
      hideResizeLocked: false,
    };
    const appWithHideResizeLockedFalse =
        await fakeHandler.addApp('', arcOptionsWithHideResizeLockedFalse);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedFalse;
    assertFalse(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| back to true hides the setting.
    const arcOptionsWithHideResizeLockedTrue = {
      type: AppType.kArc,
      hideResizeLocked: true,
    };
    const appWithHideResizeLockedTrue =
        await fakeHandler.addApp('', arcOptionsWithHideResizeLockedTrue);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedTrue;
    assertTrue(isHidden(resizeLockItem));
  });
});
