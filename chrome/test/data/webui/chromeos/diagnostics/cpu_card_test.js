// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cpu_card.js';

import {CpuUsage, RoutineType, SystemDataProviderInterface, SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCpuUsage, fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isChildVisible} from '../../test_util.m.js';

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
   * @param {!SystemInfo} systemInfo
   * @return {!Promise}
   */
  function initializeCpuCard(cpuUsage, systemInfo) {
    assertFalse(!!cpuElement);

    // Initialize the fake data.
    provider.setFakeCpuUsage(cpuUsage);
    provider.setFakeSystemInfo(systemInfo);

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
    return initializeCpuCard(fakeCpuUsage, fakeSystemInfo).then(() => {
      const dataPoints = dx_utils.getDataPointElements(cpuElement);
      dx_utils.assertTextContains(
          dataPoints[0].value,
          `${
              fakeCpuUsage[0].percentUsageUser +
              fakeCpuUsage[0].percentUsageSystem}`);
      dx_utils.assertTextContains(
          dataPoints[0].tooltipText,
          loadTimeData.getStringF('cpuUsageTooltipText', 4));
      dx_utils.assertTextContains(
          dataPoints[1].value, `${fakeCpuUsage[0].averageCpuTempCelsius}`);

      const convertkhzToGhz = (num) => parseFloat(num / 1000000).toFixed(2);
      dx_utils.assertTextContains(
          dataPoints[2].value,
          `${convertkhzToGhz(fakeCpuUsage[0].scalingCurrentFrequencyKhz)}`);
      dx_utils.assertElementContainsText(
          cpuElement.$$('#cpuChipInfo'), `${fakeSystemInfo.cpuModelName}`);
      dx_utils.assertElementContainsText(
          cpuElement.$$('#cpuChipInfo'), `${fakeSystemInfo.cpuThreadsCount}`);
      dx_utils.assertElementContainsText(
          cpuElement.$$('#cpuChipInfo'),
          `${fakeSystemInfo.cpuMaxClockSpeedKhz}`);

      const cpuChart = dx_utils.getRealtimeCpuChartElement(cpuElement);
      assertEquals(fakeCpuUsage[0].percentUsageUser, cpuChart.user);
      assertEquals(fakeCpuUsage[0].percentUsageSystem, cpuChart.system);

      // Verify the routine section is in the page.
      assertTrue(!!getRoutineSection());
      assertTrue(!!getRunTestsButton());
      assertFalse(isRunTestsButtonDisabled());

      // Verify that the data points container is visible.
      const diagnosticsCard = dx_utils.getDiagnosticsCard(cpuElement);
      assertTrue(isChildVisible(diagnosticsCard, '.data-points'));
    });
  });

  test('CpuCardUpdates', () => {
    return initializeCpuCard(fakeCpuUsage, fakeSystemInfo)
        .then(() => {
          provider.triggerCpuUsageObserver();
          return flushTasks();
        })
        .then(() => {
          const dataPoints = dx_utils.getDataPointElements(cpuElement);
          assertEquals(dataPoints[2].tooltipText, '');
        });
  });
}
