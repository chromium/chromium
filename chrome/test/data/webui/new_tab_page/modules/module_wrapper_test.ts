// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, ModuleWrapperElement} from 'chrome://new-tab-page/lazy_load.js';
import type {ModuleInstance} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createElement, initNullModule, installMock} from '../test_support.js';

suite('NewTabPageModulesModuleWrapperTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;
  let windowProxy: TestMock<WindowProxy>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      navigationStartTime: 0.0,
    });
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
  });

  function createModuleWrapper(module: ModuleInstance): ModuleWrapperElement {
    const moduleWrapper: ModuleWrapperElement = new ModuleWrapperElement();
    moduleWrapper.module = module;
    return moduleWrapper;
  }

  test('renders module descriptor', async () => {
    // Arrange.
    const moduleElement = createElement();
    moduleElement.style.height = '100px';
    const moduleWrapper = createModuleWrapper({
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
      initialized: false,
      impressed: false,
    });
    const detectedImpression =
        eventToPromise('detect-impression', moduleWrapper);
    document.body.appendChild(moduleWrapper);
    windowProxy.setResultFor('now', 123);

    // Act.
    await detectedImpression;

    // Assert.
    assertEquals(100, moduleWrapper.$.moduleElement.offsetHeight);
    assertDeepEquals(moduleElement, moduleWrapper.querySelector('div'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression', 123));
    assertEquals(1, metrics.count('NewTabPage.Modules.Impression.foo', 123));
  });

  test('receiving usage events records usage', () => {
    // Arrange.
    const moduleElement = createElement();
    const moduleWrapper = createModuleWrapper({
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
      initialized: false,
      impressed: false,
    });
    document.body.appendChild(moduleWrapper);

    // Act.
    moduleElement.dispatchEvent(new Event('usage', {bubbles: true}));

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Usage.foo'));
  });

  ['usage', 'menu-button-click'].forEach((eventName: string) => {
    test(
        `module ${eventName} event triggers onModuleUsed
              function`,
        () => {
          const moduleId = 'foo';
          const moduleElement = createElement();
          const moduleWrapper = createModuleWrapper({
            descriptor: new ModuleDescriptor(moduleId, initNullModule),
            element: moduleElement,
            initialized: false,
            impressed: false,
          });
          document.body.appendChild(moduleWrapper);

          moduleElement.dispatchEvent(new Event(eventName, {bubbles: true}));

          assertEquals(1, handler.getCallCount('onModuleUsed'));
          assertEquals(moduleId, handler.getArgs('onModuleUsed')[0]);
        });
  });

  test('clicking info button records click and module id', () => {
    // Arrange.
    const moduleElement = createElement();
    const moduleWrapper = createModuleWrapper({
      descriptor: new ModuleDescriptor('foo', initNullModule),
      element: moduleElement,
      initialized: false,
      impressed: false,
    });
    document.body.appendChild(moduleWrapper);

    // Act.
    moduleElement.dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertEquals(
        1, metrics.count('NewTabPage.Modules.InfoButtonClicked', 'foo'));
  });
});
