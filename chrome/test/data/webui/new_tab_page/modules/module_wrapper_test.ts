// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ModuleDescriptor, ModuleDescriptorV2, ModuleHeight, ModuleWrapperElement} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertThrows} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createElement, initNullModule, installMock} from '../test_support.js';

suite('NewTabPageModulesModuleWrapperTest', () => {
  let moduleWrapper: ModuleWrapperElement;
  let metrics: MetricsTracker;
  let windowProxy: TestBrowserProxy<WindowProxy>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    moduleWrapper = new ModuleWrapperElement();
    document.body.appendChild(moduleWrapper);
  });

  test('renders module descriptor', async () => {
    // Arrange.
    const moduleElement = createElement();
    moduleElement.style.height = '100px';
    const detectedImpression =
        eventToPromise('detect-impression', moduleWrapper);
    windowProxy.setResultFor('now', 123);

    // Act.
    moduleWrapper.module = {
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
    };
    await detectedImpression;

    // Assert.
    assertEquals(100, moduleWrapper.$.moduleElement.offsetHeight);
    assertDeepEquals(moduleElement, moduleWrapper.$.moduleElement.children[0]);
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression', 123));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo', 123));
  });

  test('renders fixed height descriptor', async () => {
    // Act.
    moduleWrapper.module = {
      descriptor: new ModuleDescriptorV2(
          'foo', ModuleHeight.TALL, async () => createElement()),
      element: createElement(),
    };

    // Assert.
    assertEquals(ModuleHeight.TALL, moduleWrapper.$.moduleElement.offsetHeight);
  });

  test('descriptor can only be set once', () => {
    const moduleElement = createElement();
    moduleWrapper.module = {
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
    };
    assertThrows(() => {
      moduleWrapper.module = {
        descriptor: new ModuleDescriptor('foo', initNullModule),
        element: moduleElement,
      };
    });
  });

  test('receiving usage events records usage', () => {
    // Arrange.
    const moduleElement = createElement();
    moduleWrapper.module = {
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
    };

    // Act.
    moduleElement.dispatchEvent(new Event('usage'));

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage.foo'));
  });

  test('clicking info button records click and module id', () => {
    // Arrange.
    const moduleElement = createElement();
    moduleWrapper.module = {
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
    };

    // Act.
    moduleElement.dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertEquals(
        1, metrics.count('NewTabPage.Modules.InfoButtonClicked', 'foo'));
  });
});
