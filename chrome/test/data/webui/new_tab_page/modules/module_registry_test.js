// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, ModuleRegistry, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesModuleRegistryTest', () => {
  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  /** @type {newTabPage.mojom.PageHandlerRemote} */
  let callbackRouterRemote;

  /** @type {MetricsTracker} */
  let metrics;

  setup(async () => {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    WindowProxy.setInstance(windowProxy);

    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    const callbackRouter = new newTabPage.mojom.PageCallbackRouter();
    NewTabPageProxy.setInstance(handler, callbackRouter);
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
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
    windowProxy.setResultFor('now', 5.0);

    // Act.
    const modulesPromise = ModuleRegistry.getInstance().initializeModules(0);
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
    assertEquals('foo', modules[0].id);
    assertEquals(100, modules[0].heightPx);
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].id);
    assertEquals(300, modules[1].heightPx);
    assertDeepEquals(bazModule, modules[1].element);
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
});
