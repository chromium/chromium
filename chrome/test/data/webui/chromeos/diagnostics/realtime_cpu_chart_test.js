// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/realtime_cpu_chart.js';

import {flushTasks} from 'chrome://test/test_util.m.js';
import * as diagnostics_test_utils from './diagnostics_test_utils.js';

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

    return refreshGraph();
  }

  /**
   * Promise that resolves once at least one refresh interval has passed.
   * @return {!Promise}
   */
  function refreshGraph() {
    assertTrue(!!realtimeCpuChartElement);

    return new Promise(resolve => {
      setTimeout(() => {
        flushTasks().then(() => {
          resolve();
        });
      }, realtimeCpuChartElement.refreshInterval_);
    });
  }

  test('InitializeRealtimeCpuChart', () => {
    const user = 10;
    const system = 30;
    return initializeRealtimeCpuChart(user, system).then(() => {
      diagnostics_test_utils.assertElementContainsText(
          realtimeCpuChartElement.$$('#legend-user>span'), `${user}`);
      diagnostics_test_utils.assertElementContainsText(
          realtimeCpuChartElement.$$('#legend-system>span'), `${system}`);

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

      // Chart area boundary must fit within svg.
      assertGT(
          Number(svg.getAttribute('width')),
          Number(boundary.getAttribute('width')));
      assertGT(
          Number(svg.getAttribute('height')),
          Number(boundary.getAttribute('height')));

      const chartGroup = realtimeCpuChartElement.$$('#chartGroup');

      // Margins are in effect.
      assertEquals(
          `translate(${realtimeCpuChartElement.margin_.left},${
              realtimeCpuChartElement.margin_.top})`,
          chartGroup.getAttribute('transform'));
    });
  });

  test('InitializePlot', () => {
    const user = 10;
    const system = 30;

    return initializeRealtimeCpuChart(user, system).then(() => {
      // yAxis is drawn.
      assertTrue(!!realtimeCpuChartElement.$$('#gridLines>path.domain'));

      // Correct number of yAxis ticks drawn.
      assertEquals(
          3,
          realtimeCpuChartElement.shadowRoot
              .querySelectorAll('#gridLines>g.tick')
              .length);

      // Plot lines are drawn.
      assertTrue(!!realtimeCpuChartElement.$$('#plotGroup>path.user-area')
                       .getAttribute('d'));
      assertTrue(!!realtimeCpuChartElement.$$('#plotGroup>path.system-area')
                       .getAttribute('d'));
    });
  });
});
