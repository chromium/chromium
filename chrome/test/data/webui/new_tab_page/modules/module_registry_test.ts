// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {ModuleIdName, PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {createElement, initNullModule, installMock} from '../test_support.js';

suite('NewTabPageModulesModuleRegistryTest', () => {
  let windowProxy: TestBrowserProxy<WindowProxy>;
  let handler: TestBrowserProxy<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;

  setup(async () => {
    loadTimeData.overrideValues({navigationStartTime: 0.0});
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  test('instantiates non-reordered modules', async () => {
    // Arrange.
    const fooModule = createElement();
    const bazModule = createElement();
    const bazModuleResolver: PromiseResolver<HTMLElement> =
        new PromiseResolver();
    const descriptors = [
      new ModuleDescriptor('foo', () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', initNullModule),
      new ModuleDescriptor('baz', () => bazModuleResolver.promise),
      new ModuleDescriptor('buz', () => Promise.resolve(fooModule)),
    ];
    windowProxy.setResultFor('now', 5.0);
    handler.setResultFor('getModulesIdNames', Promise.resolve({
      data: descriptors.map(d => ({id: d.id, name: d.id} as ModuleIdName)),
    }));
    handler.setResultFor('getModulesOrder', Promise.resolve({
      moduleIds: [],
    }));

    // Act.
    const moduleRegistry = new ModuleRegistry(descriptors);
    const modulesPromise = moduleRegistry.initializeModules(0);
    callbackRouterRemote.setDisabledModules(false, ['buz']);
    // Wait for first batch of modules.
    await flushTasks();
    // Move time forward to test metrics.
    windowProxy.setResultFor('now', 123.0);
    // Delayed promise resolution to test async module instantiation.
    bazModuleResolver.resolve(bazModule);
    const modules = await modulesPromise;

    // Assert.
    assertEquals(1, handler.getCallCount('updateDisabledModules'));
    assertEquals(2, modules.length);
    assertEquals('foo', modules[0]!.descriptor.id);
    assertDeepEquals(fooModule, modules[0]!.element);
    assertEquals('baz', modules[1]!.descriptor.id);
    assertDeepEquals(bazModule, modules[1]!.element);
    assertEquals(2, metrics.count('NewTabPage.Modules.Loaded'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded', 5));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded', 123));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo', 5));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.baz'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.baz', 123));
    assertEquals(2, metrics.count('NewTabPage.Modules.LoadDuration'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration', 0));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration', 118));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo', 0));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.baz'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.baz', 118));
  });

  suite('reorder', () => {
    test(
        'instantiates reordered modules without disabled modules', async () => {
          // Arrange.
          const fooModule = createElement();
          const barModule = createElement();
          const bazModule = createElement();
          const descriptors = [
            new ModuleDescriptor('foo', () => Promise.resolve(fooModule)),
            new ModuleDescriptor('bar', () => Promise.resolve(barModule)),
            new ModuleDescriptor('baz', () => Promise.resolve(bazModule)),
          ];
          handler.setResultFor('getModulesIdNames', Promise.resolve({
            data:
                descriptors.map(d => ({id: d.id, name: d.id} as ModuleIdName)),
          }));
          handler.setResultFor('getModulesOrder', Promise.resolve({
            moduleIds: ['bar', 'baz', 'foo'],
          }));

          // Act.
          const moduleRegistry = new ModuleRegistry(descriptors);
          const modulesPromise = moduleRegistry.initializeModules(0);
          callbackRouterRemote.setDisabledModules(false, []);
          // Wait for first batch of modules.
          await flushTasks();
          const modules = await modulesPromise;

          // Assert.
          assertEquals(3, modules.length);
          assertEquals('bar', modules[0]!.descriptor.id);
          assertDeepEquals(barModule, modules[0]!.element);
          assertEquals('baz', modules[1]!.descriptor.id);
          assertDeepEquals(bazModule, modules[1]!.element);
          assertEquals('foo', modules[2]!.descriptor.id);
          assertDeepEquals(fooModule, modules[2]!.element);
        });

    test('instantiates reordered modules with disabled modules', async () => {
      // Arrange.
      const fooModule = createElement();
      const barModule = createElement();
      const bazModule = createElement();
      const bizModule = createElement();
      const buzModule = createElement();
      const descriptors = [
        new ModuleDescriptor('foo', () => Promise.resolve(fooModule)),
        new ModuleDescriptor('bar', () => Promise.resolve(barModule)),
        new ModuleDescriptor('baz', () => Promise.resolve(bazModule)),
        new ModuleDescriptor('biz', () => Promise.resolve(bizModule)),
        new ModuleDescriptor('buz', () => Promise.resolve(buzModule)),
      ];
      handler.setResultFor('getModulesIdNames', Promise.resolve({
        data: descriptors.map(d => ({id: d.id, name: d.id} as ModuleIdName)),
      }));
      handler.setResultFor('getModulesOrder', Promise.resolve({
        moduleIds: ['biz', 'bar'],
      }));

      // Act.
      const moduleRegistry = new ModuleRegistry(descriptors);
      let modulesPromise = moduleRegistry.initializeModules(0);
      callbackRouterRemote.setDisabledModules(false, ['foo', 'baz', 'buz']);
      // Wait for first batch of modules with disabled modules.
      await flushTasks();
      let modules = await modulesPromise;

      // Assert.
      assertEquals(2, modules.length);
      assertEquals('biz', modules[0]!.descriptor.id);
      assertDeepEquals(bizModule, modules[0]!.element);
      assertEquals('bar', modules[1]!.descriptor.id);
      assertDeepEquals(barModule, modules[1]!.element);

      // Act.
      modulesPromise = moduleRegistry.initializeModules(0);
      callbackRouterRemote.setDisabledModules(false, []);
      // Wait for second batch of modules with re-enabled modules.
      await flushTasks();
      modules = await modulesPromise;

      // Assert.
      assertEquals(5, modules.length);
      assertEquals('biz', modules[0]!.descriptor.id);
      assertDeepEquals(bizModule, modules[0]!.element);
      assertEquals('bar', modules[1]!.descriptor.id);
      assertDeepEquals(barModule, modules[1]!.element);
      assertEquals('foo', modules[2]!.descriptor.id);
      assertDeepEquals(fooModule, modules[2]!.element);
      assertEquals('baz', modules[3]!.descriptor.id);
      assertDeepEquals(bazModule, modules[3]!.element);
      assertEquals('buz', modules[4]!.descriptor.id);
      assertDeepEquals(buzModule, modules[4]!.element);
    });
  });
});
