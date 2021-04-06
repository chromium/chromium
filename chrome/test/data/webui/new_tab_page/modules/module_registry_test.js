// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, ModuleRegistry, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertDeepEquals, assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
import {flushTasks} from '../../test_util.m.js';
import {fakeMetricsPrivate, MetricsTracker} from '../metrics_test_support.js';
import {createMock} from '../test_support.js';

/** @return {!TestBrowserProxy} */
function installMockWindowProxy() {
  const {mock, callTracker} = createMock(WindowProxy);
  WindowProxy.setInstance(mock);
  return callTracker;
}

/** @return {!TestBrowserProxy} */
function installMockHandler() {
  const {mock, callTracker} = createMock(newTabPage.mojom.PageHandlerRemote);
  NewTabPageProxy.setInstance(mock, new newTabPage.mojom.PageCallbackRouter());
  return callTracker;
}

suite('NewTabPageModulesModuleRegistryTest', () => {
  /** @type {!TestBrowserProxy} */
  let windowProxy;

  /** @type {!TestBrowserProxy} */
  let handler;

  /** @type {!newTabPage.mojom.PageRemote} */
  let callbackRouterRemote;

  /** @type {!MetricsTracker} */
  let metrics;

  setup(async () => {
    loadTimeData.overrideValues({navigationStartTime: 0.0});
    metrics = fakeMetricsPrivate();
    windowProxy = installMockWindowProxy();
    handler = installMockHandler();
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  test('instantiates modules', async () => {
    // Arrange.
    const fooModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModuleResolver = new PromiseResolver();
    ModuleRegistry.getInstance().registerModules([
      new ModuleDescriptor('foo', 'bli', () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', 'blu', () => Promise.resolve(null)),
      new ModuleDescriptor('baz', 'bla', () => bazModuleResolver.promise),
      new ModuleDescriptor('buz', 'blo', () => Promise.resolve(fooModule)),
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
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].id);
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
