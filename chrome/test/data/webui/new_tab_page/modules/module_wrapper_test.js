// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesModuleWrapperTest', () => {
  /** @type {!ModuleWrapperElement} */
  let moduleWrapper;

  /** @type {MetricsTracker} */
  let metrics;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();
    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
    moduleWrapper = document.createElement('ntp-module-wrapper');
    document.body.appendChild(moduleWrapper);
  });

  test('renders module descriptor', async () => {
    // Arrange.
    const moduleElement = document.createElement('div');
    const detectedImpression =
        eventToPromise('detect-impression', moduleWrapper);
    testProxy.setResultFor('now', 123);

    // Act.
    moduleWrapper.descriptor = {
      id: 'foo',
      heightPx: 100,
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
      heightPx: 100,
      element: moduleElement,
    };
    assertThrows(() => {
      moduleWrapper.descriptor = {
        id: 'foo',
        heightPx: 100,
        element: moduleElement,
      };
    });
  });

  test('receiving usage events records usage', () => {
    // Arrange.
    const moduleElement = document.createElement('div');
    moduleWrapper.descriptor = {
      id: 'foo',
      heightPx: 100,
      element: moduleElement,
    };

    // Act.
    moduleElement.dispatchEvent(new Event('usage'));

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage.foo'));
  });
});
