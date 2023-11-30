// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformanceAppElement} from 'chrome://performance-side-panel.top-chrome/app.js';
import {PerformancePageApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_page_api_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {$$} from 'chrome://webui-test/test_util.js';

import {TestPerformancePageApiProxy} from './test_performance_page_api_proxy.js';

suite('PerformanceControlsAppTest', () => {
  let performanceApp: PerformanceAppElement;
  let testProxy: TestPerformancePageApiProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestPerformancePageApiProxy();
    PerformancePageApiProxyImpl.setInstance(testProxy);

    performanceApp = document.createElement('performance-app');
    document.body.appendChild(performanceApp);
    await flushTasks();
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
});
