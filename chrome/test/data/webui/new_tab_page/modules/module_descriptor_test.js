// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

suite('NewTabPageModulesModuleDescriptorTest', () => {
  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /** @type {MetricsTracker} */
  let metrics;

  setup(() => {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    WindowProxy.setInstance(windowProxy);
  });

  test('instantiate module with data', async () => {
    // Arrange.
    const element = document.createElement('div');
    const moduleDescriptor = new ModuleDescriptor('foo', 'bar', () => {
      // Move time forward to simulate delay instantiating module.
      windowProxy.setResultFor('now', 128);
      return Promise.resolve(element);
    });
    windowProxy.setResultFor('now', 123);

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(element, moduleDescriptor.element);
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded', 128));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo', 128));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration', 5));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo', 5));
  });

  test('instantiate module without data', async () => {
    // Arrange.
    const moduleDescriptor =
        new ModuleDescriptor('foo', 'bar', () => Promise.resolve(null));

    // Act.
    await moduleDescriptor.initialize();

    // Assert.
    assertEquals(null, moduleDescriptor.element);
    assertEquals(0, metrics.count('NewTabPage.Modules.Loaded'));
    assertEquals(0, metrics.count('NewTabPage.Modules.Loaded.foo'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadDuration'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadDuration.foo'));
  });

  test('module load times out', async () => {
    // Arrange.
    const moduleDescriptor = new ModuleDescriptor(
        'foo', 'bar', () => new Promise(() => {}) /* Never resolves. */);

    // Act.
    const initializePromise = moduleDescriptor.initialize(123);
    const [callback, timeout] = await windowProxy.whenCalled('setTimeout');
    callback();
    await initializePromise;

    // Assert.
    assertEquals(null, moduleDescriptor.element);
    assertEquals(123, timeout);
  });
});
