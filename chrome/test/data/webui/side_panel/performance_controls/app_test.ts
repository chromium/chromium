// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformanceAppElement} from 'chrome://performance-side-panel.top-chrome/app.js';
import {PerformanceSidePanelNotification} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformancePageApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_page_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {$$} from 'chrome://webui-test/test_util.js';

import {TestPerformancePageApiProxy} from './test_performance_page_api_proxy.js';

suite('PerformanceControlsAppTest', () => {
  let performanceApp: PerformanceAppElement;
  let testProxy: TestPerformancePageApiProxy;

  async function initializePerformanceApp(
      notifications: PerformanceSidePanelNotification[]) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestPerformancePageApiProxy();
    PerformancePageApiProxyImpl.setInstance(testProxy);

    loadTimeData.overrideValues({
      sidePanelNotifications: notifications.map(n => `${n}`).join(','),
    });

    performanceApp = document.createElement('performance-app');
    document.body.appendChild(performanceApp);

    await flushTasks();
  }

  suite('with no notification', () => {
    setup(async () => {
      await initializePerformanceApp([]);
    });

    test('app contains browser health card', async () => {
      assertTrue(!!$$(performanceApp, '#browserHealthCard'));
    });

    test('app contains memory saver card', async () => {
      assertTrue(!!$$(performanceApp, '#memorySaverCard'));
    });

    test('app contains battery saver card', async () => {
      assertTrue(!!$$(performanceApp, '#batterySaverCard'));
    });

    test('app calls showUi', async () => {
      assertEquals(testProxy.getCallCount('showUi'), 1);
    });

    test('browser health card is first', async () => {
      const cards = [...$$(performanceApp, '.cards')!.children];
      assertLT(
          cards.indexOf($$(performanceApp, '#browserHealthCard')!),
          cards.indexOf($$(performanceApp, '#memorySaverCard')!),
          'Browser health card should be before memory saver');
      assertLT(
          cards.indexOf($$(performanceApp, '#browserHealthCard')!),
          cards.indexOf($$(performanceApp, '#batterySaverCard')!),
          'Browser health card should be before battery saver');
    });
  });

  suite('with memory saver notification', () => {
    setup(async () => {
      await initializePerformanceApp(
          [PerformanceSidePanelNotification.kMemorySaverRevisitDiscardedTab]);
    });

    test('memory saver card is first', async () => {
      const cards = [...$$(performanceApp, '.cards')!.children];
      assertLT(
          cards.indexOf($$(performanceApp, '#memorySaverCard')!),
          cards.indexOf($$(performanceApp, '#browserHealthCard')!),
          'Memory saver card should be first');
    });
  });



});
