// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformanceAppElement} from 'chrome://performance-side-panel.top-chrome/app.js';
import {PerformanceApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPerformanceApiProxy} from './test_performance_api_proxy.js';

suite('PerformanceControlsAppTest', () => {
  let performanceApp: PerformanceAppElement;
  let testProxy: TestPerformanceApiProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestPerformanceApiProxy();
    PerformanceApiProxyImpl.setInstance(testProxy);

    performanceApp = document.createElement('performance-app');
    document.body.appendChild(performanceApp);
    await flushTasks();
  });

  test('app is empty', async () => {
    assertEquals(performanceApp.innerHTML, '');
  });
});
