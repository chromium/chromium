// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/cpu_card.js';

import {fakeCpuUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flushTasks} from 'chrome://test/test_util.m.js';
import * as dx_utils from './diagnostics_test_utils.js';

suite('CpuCardTest', () => {
  /** @type {?HTMLElement} */
  let cpuElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    if (cpuElement) {
      cpuElement.remove();
    }
    cpuElement = null;
    provider.reset();
  });

  /**
   * @param {!CpuUsage} cpuUsage
   * @return {!Promise}
   */
  function initializeCpuCard(cpuUsage) {
    assertFalse(!!cpuElement);

    // Initialize the fake data.
    provider.setFakeCpuUsage(cpuUsage);

    // Add the CPU card to the DOM.
    cpuElement = document.createElement('cpu-card');
    assertTrue(!!cpuElement);
    document.body.appendChild(cpuElement);

    return flushTasks();
  }

  test('CpuCardPopulated', () => {
    return initializeCpuCard(fakeCpuUsage).then(() => {
      const dataPoints = dx_utils.getDataPointElements(cpuElement);
      const currentlyUsingValue = fakeCpuUsage[0].percent_usage_user +
          fakeCpuUsage[0].percent_usage_system;
      assertEquals(currentlyUsingValue, dataPoints[0].value);
      assertEquals(
          fakeCpuUsage[0].cpu_temp_degrees_celcius, dataPoints[1].value);

      const cpuChart = dx_utils.getRealtimeCpuChartElement(cpuElement);
      assertEquals(fakeCpuUsage[0].percent_usage_user, cpuChart.user);
      assertEquals(fakeCpuUsage[0].percent_usage_system, cpuChart.system);
    });
  });
});