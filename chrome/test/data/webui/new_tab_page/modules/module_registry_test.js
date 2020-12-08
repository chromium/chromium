// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/new_tab_page.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

suite('NewTabPageModulesModuleRegistryTest', () => {
  test('instantiates modules', async () => {
    // Arrange.
    const fooModule = document.createElement('div');
    const bazModule = document.createElement('div');
    const bazModuleResolver = new PromiseResolver();
    ModuleRegistry.getInstance().registerModules([
      new ModuleDescriptor('foo', 100, () => Promise.resolve({
        element: fooModule,
        title: 'Foo Title',
      })),
      new ModuleDescriptor('bar', 200, () => null),
      new ModuleDescriptor('baz', 300, () => bazModuleResolver.promise),
    ]);

    // Act.
    const modulesPromise = ModuleRegistry.getInstance().initializeModules();
    // Delayed promise resolution to test async module instantiation.
    bazModuleResolver.resolve({
      element: bazModule,
      title: 'Baz Title',
    });
    const modules = await modulesPromise;

    // Assert.
    assertEquals(2, modules.length);
    assertEquals('foo', modules[0].id);
    assertEquals(100, modules[0].heightPx);
    assertEquals('Foo Title', modules[0].title);
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].id);
    assertEquals(300, modules[1].heightPx);
    assertEquals('Baz Title', modules[1].title);
    assertDeepEquals(bazModule, modules[1].element);
  });
});
