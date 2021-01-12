// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, ModuleDescriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageModulesModuleDescriptorTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
  });

  test('instantiate module with data', async () => {
    // Arrange.
    const element = document.createElement('div');
    const moduleDescriptor =
        new ModuleDescriptor('foo', 100, () => Promise.resolve(element));
    testProxy.setResultFor('now', 123);

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(element, moduleDescriptor.element);
    const [id, now] = await testProxy.handler.whenCalled('onModuleLoaded');
    assertEquals(1, testProxy.handler.getCallCount('onModuleLoaded'));
    assertEquals('foo', id);
    assertEquals(123, now);
  });

  test('instantiate module without data', async () => {
    // Arrange.
    const moduleDescriptor =
        new ModuleDescriptor('foo', 100, () => Promise.resolve(null));

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(null, moduleDescriptor.element);
    assertEquals(0, testProxy.handler.getCallCount('onModuleLoaded'));
  });
});
