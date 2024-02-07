// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {createElement, initNullModule, installMock} from '../test_support.js';

suite('NewTabPageModulesModuleDescriptorTest', () => {
  let windowProxy: TestMock<WindowProxy>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
  });

  test('instantiate module with data', async () => {
    // Arrange.
    const element = createElement();
    const moduleDescriptor = new ModuleDescriptor('foo', () => {
      // Move time forward to simulate delay instantiating module.
      windowProxy.setResultFor('now', 128);
      return Promise.resolve(element);
    });
    windowProxy.setResultFor('now', 123);

    // Act.
    const moduleElement = await moduleDescriptor.initialize(0);

    // Assert.
    assertEquals(element, moduleElement);
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
    const moduleDescriptor = new ModuleDescriptor('foo', initNullModule);

    // Act.
    const moduleElement = await moduleDescriptor.initialize(0);

    // Assert.
    assertEquals(null, moduleElement);
    assertEquals(0, metrics.count('NewTabPage.Modules.Loaded'));
    assertEquals(0, metrics.count('NewTabPage.Modules.Loaded.foo'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadDuration'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadDuration.foo'));
  });

  test('module load times out', async () => {
    // Arrange.
    const moduleDescriptor = new ModuleDescriptor(
        'foo', () => new Promise(() => {}) /* Never resolves. */);

    // Act.
    const initializePromise = moduleDescriptor.initialize(123);
    const [callback, timeout] = await windowProxy.whenCalled('setTimeout');
    callback();
    const moduleElement = await initializePromise;

    // Assert.
    assertEquals(null, moduleElement);
    assertEquals(123, timeout);
  });
});
