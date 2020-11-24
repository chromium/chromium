// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/realtime_cpu_chart.js';

import {assertEquals, assertFalse, assertGT, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as diagnostics_test_utils from './diagnostics_test_utils.js';

export function realtimeCpuChartTestSuite() {
  /** @type {?RealtimeCpuChartElement} */
  let realtimeCpuChartElement = null;

  setup(() => {
    document.body.innerHTML = '';
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
    realtimeCpuChartElement = /** @type {!RealtimeCpuChartElement} */ (
        document.createElement('realtime-cpu-chart'));
    assertTrue(!!realtimeCpuChartElement);
    document.body.appendChild(realtimeCpuChartElement);
    realtimeCpuChartElement.user = user;
    realtimeCpuChartElement.system = system;

    return refreshGraph();
  }

  /**
   * Get frameDuration_ private member for testing.
   * @suppress {visibility} // access private member
   */
  function getFrameDuration() {
    assertTrue(!!realtimeCpuChartElement);
    return realtimeCpuChartElement.frameDuration_;
  }

  /**
   * Get padding_ private member for testing.
   * @suppress {visibility} // access private member
   */
  function getPaddings() {
    assertTrue(!!realtimeCpuChartElement);
    return realtimeCpuChartElement.padding_;
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
      }, getFrameDuration());
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
          `translate(${getPaddings().left},${getPaddings().top})`,
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
          5,
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
}
