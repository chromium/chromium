// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/realtime_cpu_chart.js';

import {flushTasks} from 'chrome://test/test_util.m.js';

suite('RealtimeCpuChartTest', () => {
  /** @type {?HTMLElement} */
  let realtimeCpuChartElement = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (realtimeCpuChartElement) {
      realtimeCpuChartElement.remove();
    }
    realtimeCpuChartElement = null;
  });

  /**
   * @param {number} user
   * @param {number} system
   * @return {!Promise}
   */
  function initializeRealtimeCpuChart(user, system) {
    assertFalse(!!realtimeCpuChartElement);

    // Add the element to the DOM.
    realtimeCpuChartElement = document.createElement('realtime-cpu-chart');
    assertTrue(!!realtimeCpuChartElement);
    document.body.appendChild(realtimeCpuChartElement);
    realtimeCpuChartElement.user = user;
    realtimeCpuChartElement.system = system;
    return flushTasks();
  }

  test('InitializeRealtimeCpuChart', () => {
    const user = 10;
    const system = 30;
    return initializeRealtimeCpuChart(user, system).then(() => {
      assertEquals(
          `${user}`,
          realtimeCpuChartElement.$$('#legend-user>span').textContent.trim());
      assertEquals(
          `${system}`,
          realtimeCpuChartElement.$$('#legend-system>span').textContent.trim());

      assertEquals(user, realtimeCpuChartElement.user);
      assertEquals(system, realtimeCpuChartElement.system);
    });
  });

  test('ChartAreaBoundary', () => {
    const user = 10;
    const system = 30;
    return initializeRealtimeCpuChart(user, system).then(() => {
      const svg = realtimeCpuChartElement.$$('#chart');
      const boundary = realtimeCpuChartElement.$$('#defClip>rect');
      assertGT(
          Number(svg.getAttribute('width')),
          Number(boundary.getAttribute('width')));
      assertGT(
          Number(svg.getAttribute('height')),
          Number(boundary.getAttribute('height')));
    });
  });
});
