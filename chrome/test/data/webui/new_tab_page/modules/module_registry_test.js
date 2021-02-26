// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/new_tab_page.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageModulesModuleRegistryTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(async () => {
    testProxy = createTestProxy();
    testProxy.setResultFor('now', 0);
    BrowserProxy.instance_ = testProxy;
  });

  test('instantiates modules', async () => {
    // Arrange.
    const fooModule = document.createElement('div');
    const bazModule = document.createElement('div');
    const bazModuleResolver = new PromiseResolver();
    ModuleRegistry.getInstance().registerModules([
      new ModuleDescriptor('foo', 'bli', 100, () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', 'blu', 200, () => null),
      new ModuleDescriptor('baz', 'bla', 300, () => bazModuleResolver.promise),
      new ModuleDescriptor('buz', 'blo', 400, () => Promise.resolve(fooModule)),
    ]);

    // Act.
    const modulesPromise = ModuleRegistry.getInstance().initializeModules(0);
    testProxy.callbackRouterRemote.setDisabledModules(false, ['buz']);
    // Delayed promise resolution to test async module instantiation.
    bazModuleResolver.resolve(bazModule);
    const modules = await modulesPromise;

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('updateDisabledModules'));
    assertEquals(2, modules.length);
    assertEquals('foo', modules[0].id);
    assertEquals(100, modules[0].heightPx);
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].id);
    assertEquals(300, modules[1].heightPx);
    assertDeepEquals(bazModule, modules[1].element);
  });
});
