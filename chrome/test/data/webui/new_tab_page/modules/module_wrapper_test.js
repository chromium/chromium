// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesModuleWrapperTest', () => {
  /** @type {!ModuleWrapperElement} */
  let moduleWrapper;

  /** @type {MetricsTracker} */
  let metrics;

  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  setup(() => {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    WindowProxy.setInstance(windowProxy);
    moduleWrapper = document.createElement('ntp-module-wrapper');
    document.body.appendChild(moduleWrapper);
  });

  test('renders module descriptor', async () => {
    // Arrange.
    const moduleElement = document.createElement('div');
    moduleElement.style.height = '100px';
    const detectedImpression =
        eventToPromise('detect-impression', moduleWrapper);
    windowProxy.setResultFor('now', 123);

    // Act.
    moduleWrapper.descriptor = {
      id: 'foo',
      element: moduleElement,
    };
    await detectedImpression;

    // Assert.
    assertEquals(100, $$(moduleWrapper, '#moduleElement').offsetHeight);
    assertDeepEquals(
        moduleElement, $$(moduleWrapper, '#moduleElement').children[0]);
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression', 123));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo', 123));
  });

  test('descriptor can only be set once', () => {
    const moduleElement = document.createElement('div');
    moduleWrapper.descriptor = {
      id: 'foo',
      element: moduleElement,
    };
    assertThrows(() => {
      moduleWrapper.descriptor = {
        id: 'foo',
        element: moduleElement,
      };
    });
  });

  test('receiving usage events records usage', () => {
    // Arrange.
    const moduleElement = document.createElement('div');
    moduleWrapper.descriptor = {
      id: 'foo',
      element: moduleElement,
    };

    // Act.
    moduleElement.dispatchEvent(new Event('usage'));

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage.foo'));
  });
});
