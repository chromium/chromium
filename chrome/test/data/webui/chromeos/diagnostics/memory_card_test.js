// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/memory_card.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {convertKibToGibDecimalString} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeMemoryUsage, fakeMemoryUsageLowAvailableMemory} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {MemoryCardElement} from 'chrome://diagnostics/memory_card.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {MemoryUsage} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isChildVisible, isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('memoryCardTestSuite', function() {
  /** @type {?MemoryCardElement} */
  let memoryElement = null;

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
    if (memoryElement) {
      memoryElement.remove();
    }
    memoryElement = null;
    provider.reset();
  });

  /**
   * @param {!Array<MemoryUsage>} memoryUsage
   * @return {!Promise}
   */
  function initializeMemoryCard(memoryUsage) {
    assertFalse(!!memoryElement);

    // Initialize the fake data.
    provider.setFakeMemoryUsage(memoryUsage);

    // Add the memory card to the DOM.
    memoryElement = /** @type {!MemoryCardElement} */ (
        document.createElement('memory-card'));
    assertTrue(!!memoryElement);
    document.body.appendChild(memoryElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   * @return {!RoutineSectionElement}
   */
  function getRoutineSection() {
    const routineSection = /** @type {!RoutineSectionElement} */ (
        memoryElement.shadowRoot.querySelector('routine-section'));
    assertTrue(!!routineSection);
    return routineSection;
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   * @return {!CrButtonElement}
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

  test('MemoryCardPopulated', () => {
    return initializeMemoryCard(fakeMemoryUsage).then(() => {
      const barChart = dx_utils.getPercentBarChartElement(memoryElement);
      const memInUse = fakeMemoryUsage[0].totalMemoryKib -
          fakeMemoryUsage[0].availableMemoryKib;
      const expectedChartHeader = loadTimeData.getStringF(
          'memoryAvailable',
          convertKibToGibDecimalString(
              fakeMemoryUsage[0].availableMemoryKib, 2),
          convertKibToGibDecimalString(fakeMemoryUsage[0].totalMemoryKib, 2));
      assertEquals(fakeMemoryUsage[0].totalMemoryKib, barChart.max);
      assertEquals(memInUse, barChart.value);
      dx_utils.assertTextContains(barChart.header, expectedChartHeader);

      // Verify the routine section is in the card.
      assertTrue(!!getRoutineSection());
      assertTrue(!!getRunTestsButton());
      assertFalse(isRunTestsButtonDisabled());

      // Verify that the data points container is not visible.
      const diagnosticsCard = dx_utils.getDiagnosticsCard(memoryElement);
      assertFalse(isChildVisible(diagnosticsCard, '.data-points'));
    });
  });

  test('TestDisabledWhenAvailableMemoryLessThan500MB', () => {
    return initializeMemoryCard(fakeMemoryUsageLowAvailableMemory).then(() => {
      const routineSectionElement = getRoutineSection();
      assertEquals(
          routineSectionElement.additionalMessage,
          loadTimeData.getString('notEnoughAvailableMemoryMessage'));
      assertTrue(isRunTestsButtonDisabled());
      assertTrue(isVisible(/** @type {!HTMLElement} */ (
          routineSectionElement.shadowRoot.querySelector('#messageIcon'))));
    });
  });
});
