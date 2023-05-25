// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cpu_card.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CpuCardElement} from 'chrome://diagnostics/cpu_card.js';
import {fakeCpuUsage, fakeMemoryUsage, fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {CpuUsage, SystemDataProviderInterface, SystemInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {RoutineType} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isChildVisible, isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('cpuCardTestSuite', function() {
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
   * @param {!Array<MemoryUsage>} memoryUsage
   * @return {!Promise}
   */
  function initializeCpuCard(cpuUsage, systemInfo, memoryUsage) {
    assertFalse(!!cpuElement);

    // Initialize the fake data.
    provider.setFakeCpuUsage(cpuUsage);
    provider.setFakeSystemInfo(systemInfo);
    provider.setFakeMemoryUsage(memoryUsage);

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
            cpuElement.shadowRoot.querySelector('routine-section'));
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
    const highMemoryAvailable = [{
      availableMemoryKib: 57000000,
      freeMemoryKib: 15000000,
      totalMemoryKib: 128000000,
    }];
    return initializeCpuCard(fakeCpuUsage, fakeSystemInfo, highMemoryAvailable)
        .then(() => {
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(cpuElement, '#cpuUsageUser'),
              `${
                  fakeCpuUsage[0].percentUsageUser +
                  fakeCpuUsage[0].percentUsageSystem}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(cpuElement, '#cpuUsageUser').tooltipText,
              loadTimeData.getStringF('cpuUsageTooltipText', 4));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(cpuElement, '#cpuTemp'),
              `${fakeCpuUsage[0].averageCpuTempCelsius}`);

          const convertkhzToGhz = (num) => parseFloat(num / 1000000).toFixed(2);
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(cpuElement, '#cpuSpeed'),
              `${convertkhzToGhz(fakeCpuUsage[0].scalingCurrentFrequencyKhz)}`);
          dx_utils.assertElementContainsText(
              cpuElement.shadowRoot.querySelector('#cpuChipInfo'),
              `${fakeSystemInfo.cpuModelName}`);
          dx_utils.assertElementContainsText(
              cpuElement.shadowRoot.querySelector('#cpuChipInfo'),
              `${fakeSystemInfo.cpuThreadsCount}`);
          dx_utils.assertElementContainsText(
              cpuElement.shadowRoot.querySelector('#cpuChipInfo'),
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
    return initializeCpuCard(fakeCpuUsage, fakeSystemInfo, fakeMemoryUsage)
        .then(() => {
          provider.triggerCpuUsageObserver();
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              dx_utils.getDataPoint(cpuElement, '#cpuSpeed').tooltipText, '');
        });
  });

  test('TestsDisabledWhenAvailableMemoryLessThan625MB', () => {
    const lowMemoryAvailable = [{
      availableMemoryKib: 57000,
      freeMemoryKib: 15000,
      totalMemoryKib: 128000,
    }];
    return initializeCpuCard(fakeCpuUsage, fakeSystemInfo, lowMemoryAvailable)
        .then(() => {
          const routineSectionElement = getRoutineSection();
          assertEquals(
              routineSectionElement.additionalMessage,
              loadTimeData.getString('notEnoughAvailableMemoryCpuMessage'));
          assertTrue(isRunTestsButtonDisabled());
          assertTrue(isVisible(/** @type {!HTMLElement} */ (
              routineSectionElement.shadowRoot.querySelector('#messageIcon'))));
        });
  });
});
