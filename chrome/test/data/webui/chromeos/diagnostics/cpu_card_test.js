// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cpu_card.js';

import {CpuUsage, RoutineName, SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCpuUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function cpuCardTestSuite() {
  /** @type {?CpuCardElement} */
  let cpuElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (cpuElement) {
      cpuElement.remove();
    }
    cpuElement = null;
    provider.reset();
  });

  /**
   * @param {!Array<!CpuUsage>} cpuUsage
   * @return {!Promise}
   */
  function initializeCpuCard(cpuUsage) {
    assertFalse(!!cpuElement);

    // Initialize the fake data.
    provider.setFakeCpuUsage(cpuUsage);

    // Add the CPU card to the DOM.
    cpuElement =
        /** @type {!CpuCardElement} */ (document.createElement('cpu-card'));
    assertTrue(!!cpuElement);
    document.body.appendChild(cpuElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   * @return {!RoutineSectionElement}
   */
  function getRoutineSection() {
    const routineSection =
        /** @type {!RoutineSectionElement} */ (
            cpuElement.$$('routine-section'));
    assertTrue(!!routineSection);
    return routineSection;
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   * @return {!HTMLElement}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {boolean}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
  }

  test('CpuCardPopulated', () => {
    return initializeCpuCard(fakeCpuUsage).then(() => {
      const dataPoints = dx_utils.getDataPointElements(cpuElement);
      const currentlyUsingValue =
          fakeCpuUsage[0].percentUsageUser + fakeCpuUsage[0].percentUsageSystem;
      assertEquals(currentlyUsingValue, dataPoints[0].value);
      assertEquals(fakeCpuUsage[0].averageCpuTempCelsius, dataPoints[1].value);

      const cpuChart = dx_utils.getRealtimeCpuChartElement(cpuElement);
      assertEquals(fakeCpuUsage[0].percentUsageUser, cpuChart.user);
      assertEquals(fakeCpuUsage[0].percentUsageSystem, cpuChart.system);

      // Verify the routine section is in the page.
      assertTrue(!!getRoutineSection());
      assertTrue(!!getRunTestsButton());
      assertFalse(isRunTestsButtonDisabled());
    });
  });
}
