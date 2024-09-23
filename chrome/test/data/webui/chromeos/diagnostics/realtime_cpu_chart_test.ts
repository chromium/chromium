// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/realtime_cpu_chart.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ChartPadding, RealtimeCpuChartElement} from 'chrome://diagnostics/realtime_cpu_chart.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import * as diagnostics_test_utils from './diagnostics_test_utils.js';

suite('realtimeCpuChartTestSuite', function() {
  let realtimeCpuChartElement: RealtimeCpuChartElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    realtimeCpuChartElement?.remove();
    realtimeCpuChartElement = null;
  });

  function initializeRealtimeCpuChart(
      user: number, system: number): Promise<void> {
    // Add the element to the DOM.
    realtimeCpuChartElement =
        document.createElement(RealtimeCpuChartElement.is);
    assert(realtimeCpuChartElement);
    document.body.appendChild(realtimeCpuChartElement);
    realtimeCpuChartElement.user = user;
    realtimeCpuChartElement.system = system;

    return refreshGraph();
  }

  /**
   * Get frameDuration_ private member for testing.
   */
  function getFrameDuration() {
    assert(realtimeCpuChartElement);
    return realtimeCpuChartElement.getFrameDurationForTesting();
  }

  /**
   * Get padding_ private member for testing.
   */
  function getPaddings(): ChartPadding {
    assert(realtimeCpuChartElement);
    return realtimeCpuChartElement.getPaddingForTesting();
  }

  /**
   * Promise that resolves once at least one refresh interval has passed.
   */
  function refreshGraph(): Promise<void> {
    assert(realtimeCpuChartElement);

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
      assert(realtimeCpuChartElement);
      diagnostics_test_utils.assertElementContainsText(
          realtimeCpuChartElement.shadowRoot!.querySelector(
              '#legend-user>span'),
          `${user}`);
      diagnostics_test_utils.assertElementContainsText(
          realtimeCpuChartElement.shadowRoot!.querySelector(
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
      assert(realtimeCpuChartElement);
      const svg = realtimeCpuChartElement!.shadowRoot!.querySelector('#chart');
      assert(svg);
      const boundary =
          realtimeCpuChartElement.shadowRoot!.querySelector('#defClip>rect');
      assert(boundary);
      // Chart area boundary must fit within svg.
      assertGT(
          Number(svg.getAttribute('width')),
          Number(boundary.getAttribute('width')));
      assertGT(
          Number(svg.getAttribute('height')),
          Number(boundary.getAttribute('height')));

      const chartGroup =
          realtimeCpuChartElement.shadowRoot!.querySelector('#chartGroup');
      assert(chartGroup);
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
      assert(realtimeCpuChartElement);
      // yAxis is drawn.
      assertTrue(!!realtimeCpuChartElement.shadowRoot!.querySelector(
          '#gridLines>path.domain'));

      // Correct number of yAxis ticks drawn.
      assertEquals(
          5,
          realtimeCpuChartElement.shadowRoot!
              .querySelectorAll('#gridLines>g.tick')
              .length);

      // Plot lines are drawn.
      assertTrue(
          !!realtimeCpuChartElement.shadowRoot!
                .querySelector('#plotGroup>path.user-area')!.getAttribute('d'));
      assertTrue(!!realtimeCpuChartElement.shadowRoot!
                       .querySelector(
                           '#plotGroup>path.system-area')!.getAttribute('d'));
    });
  });
});
