// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cpu_card.js';

import {fakeCpuUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flushTasks} from 'chrome://test/test_util.m.js';
import * as dx_utils from './diagnostics_test_utils.js';

export function cpuCardTestSuite() {
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

  /**
   * Returns the routine-section from the card.
   * @return {!RoutineSection}
   */
  function getRoutineSection() {
    const routineSection = cpuElement.$$('routine-section');
    assertTrue(!!routineSection);
    return routineSection;
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   * @return {!CrButton}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {bool}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
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

      // Verify the routine section is in the page.
      assertTrue(!!getRoutineSection());
      assertTrue(!!getRunTestsButton());
      assertFalse(isRunTestsButtonDisabled());
    });
  });
}
