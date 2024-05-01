// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/memory_card.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {convertKibToGibDecimalString} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeMemoryUsage, fakeMemoryUsageLowAvailableMemory} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {MemoryCardElement} from 'chrome://diagnostics/memory_card.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {MemoryUsage} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('memoryCardTestSuite', function() {
  let memoryElement: MemoryCardElement|null = null;

  const provider = new FakeSystemDataProvider();

  suiteSetup(() => {
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    memoryElement?.remove();
    memoryElement = null;
    provider.reset();
  });

  function initializeMemoryCard(memoryUsage: MemoryUsage[]): Promise<void> {
    assertFalse(!!memoryElement);

    // Initialize the fake data.
    provider.setFakeMemoryUsage(memoryUsage);

    // Add the memory card to the DOM.
    memoryElement = document.createElement('memory-card');
    assert(memoryElement);
    document.body.appendChild(memoryElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   */
  function getRoutineSection(): RoutineSectionElement {
    assert(memoryElement);
    return strictQuery('routine-section', memoryElement.shadowRoot, RoutineSectionElement);
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   */
  function getRunTestsButton(): CrButtonElement {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assert(button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   */
  function isRunTestsButtonDisabled(): boolean {
    return getRunTestsButton().disabled;
  }

  test('MemoryCardPopulated', () => {
    return initializeMemoryCard(fakeMemoryUsage).then(() => {
      const barChart = dx_utils.getPercentBarChartElement(memoryElement);
      const memInUse = fakeMemoryUsage[0]!.totalMemoryKib -
          fakeMemoryUsage[0]!.availableMemoryKib;
      const expectedChartHeader = loadTimeData.getStringF(
          'memoryAvailable',
          convertKibToGibDecimalString(
              fakeMemoryUsage[0]!.availableMemoryKib, 2),
          convertKibToGibDecimalString(fakeMemoryUsage[0]!.totalMemoryKib, 2));
      assertEquals(fakeMemoryUsage[0]!.totalMemoryKib, barChart.max);
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
      assertTrue(isVisible(
          routineSectionElement!.shadowRoot!.querySelector('#messageIcon')));
    });
  });
});
