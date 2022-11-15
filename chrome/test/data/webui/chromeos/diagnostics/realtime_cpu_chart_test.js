// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/realtime_cpu_chart.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {RealtimeCpuChartElement} from 'chrome://diagnostics/realtime_cpu_chart.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import * as diagnostics_test_utils from './diagnostics_test_utils.js';

suite('realtimeCpuChartTestSuite', function() {
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
          realtimeCpuChartElement.shadowRoot.querySelector('#legend-user>span'),
          `${user}`);
      diagnostics_test_utils.assertElementContainsText(
          realtimeCpuChartElement.shadowRoot.querySelector(
              '#legend-system>span'),
          `${system}`);

      assertEquals(user, realtimeCpuChartElement.user);
      assertEquals(system, realtimeCpuChartElement.system);
    });
  });

  test('ChartAreaBoundary', () => {
    const user = 10;
    const system = 30;
    return initializeRealtimeCpuChart(user, system).then(() => {
      const svg = realtimeCpuChartElement.shadowRoot.querySelector('#chart');
      const boundary =
          realtimeCpuChartElement.shadowRoot.querySelector('#defClip>rect');

      // Chart area boundary must fit within svg.
      assertGT(
          Number(svg.getAttribute('width')),
          Number(boundary.getAttribute('width')));
      assertGT(
          Number(svg.getAttribute('height')),
          Number(boundary.getAttribute('height')));

      const chartGroup =
          realtimeCpuChartElement.shadowRoot.querySelector('#chartGroup');

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
      assertTrue(!!realtimeCpuChartElement.shadowRoot.querySelector(
          '#gridLines>path.domain'));

      // Correct number of yAxis ticks drawn.
      assertEquals(
          5,
          realtimeCpuChartElement.shadowRoot
              .querySelectorAll('#gridLines>g.tick')
              .length);

      // Plot lines are drawn.
      assertTrue(!!realtimeCpuChartElement.shadowRoot
                       .querySelector('#plotGroup>path.user-area')
                       .getAttribute('d'));
      assertTrue(!!realtimeCpuChartElement.shadowRoot
                       .querySelector('#plotGroup>path.system-area')
                       .getAttribute('d'));
    });
  });
});
