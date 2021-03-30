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
    const moduleDescriptor = new ModuleDescriptor('foo', 'bar', 100, () => {
      // Move time forward to simulate delay instantiating module.
      testProxy.setResultFor('now', 128);
      return Promise.resolve(element);
    });
    testProxy.setResultFor('now', 123);

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(element, moduleDescriptor.element);
    assertEquals(1, testProxy.handler.getCallCount('onModuleLoaded'));
    const [[id, delta, now]] = testProxy.handler.getArgs('onModuleLoaded');
    assertEquals('foo', id);
    assertEquals(128, now);
    assertEquals(5000n, delta.microseconds);  // 128ms - 123ms === 5000µs.
  });

  test('instantiate module without data', async () => {
    // Arrange.
    const moduleDescriptor =
        new ModuleDescriptor('foo', 'bar', 100, () => Promise.resolve(null));

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(null, moduleDescriptor.element);
    assertEquals(0, testProxy.handler.getCallCount('onModuleLoaded'));
  });

  test('module load times out', async () => {
    // Arrange.
    const moduleDescriptor = new ModuleDescriptor(
        'foo', 'bar', 100, () => new Promise(() => {}) /* Never resolves. */);

    // Act.
    const initializePromise = moduleDescriptor.initialize(123);
    const [callback, timeout] = await testProxy.whenCalled('setTimeout');
    callback();
    await initializePromise;

    // Assert.
    assertEquals(null, moduleDescriptor.element);
    assertEquals(123, timeout);
  });

  test('module update height in initialization', async () => {
    // Arrange.
    const element = document.createElement('div');
    let moduleDescriptor = new ModuleDescriptor('foo', 'bar', 100, () => {
      element.height = 200;
      return Promise.resolve(element);
    });
    testProxy.setResultFor('now', 123);

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(element, moduleDescriptor.element);
    assertEquals(200, moduleDescriptor.heightPx);
  });
});
