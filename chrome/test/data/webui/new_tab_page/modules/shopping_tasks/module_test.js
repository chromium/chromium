// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {shoppingTasksDescriptor, ShoppingTasksHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

suite('NewTabPageModulesShoppingTasksModuleTest', () => {
  /**
   * @implements {ShoppingTasksHandlerProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(ShoppingTasksHandlerProxy);
    testProxy.handler = TestBrowserProxy.fromClass(
        shoppingTasks.mojom.ShoppingTasksHandlerRemote);
    testProxy.handler.setResultFor(
        'getPrimaryShoppingTask', Promise.resolve(null));
    ShoppingTasksHandlerProxy.instance_ = testProxy;
  });

  test('creates module', async () => {
    // Act.
    await shoppingTasksDescriptor.initialize();
    const module = shoppingTasksDescriptor.element;
    document.body.append(module);

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryShoppingTask'));
  });
});
