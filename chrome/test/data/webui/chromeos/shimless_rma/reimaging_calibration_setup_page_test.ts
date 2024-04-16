// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationSetupPage} from 'chrome://shimless-rma/reimaging_calibration_setup_page.js';
import {CalibrationSetupInstruction, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('reimagingCalibrationSetupPageTest', function() {
  let component: ReimagingCalibrationSetupPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  const instructionsSelector = '#instructions';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeCalibrationPage(instructions: CalibrationSetupInstruction):
      Promise<void> {
    service.setGetCalibrationSetupInstructionsResult(instructions);

    assert(!component);
    component = document.createElement(ReimagingCalibrationSetupPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the page initializes.
  test('Initializes', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    assert(component);
    assertFalse(
        strictQuery(instructionsSelector, component.shadowRoot, HTMLElement)
            .hidden);
  });

  // Verify clicking the next button triggers run calibration..
  test('NextButtonTriggersRunCalibrationStep', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    const expectedPromise = new PromiseResolver<{stateResult: StateResult}>();
    service.runCalibrationStep = () => expectedPromise.promise;

    assert(component);
    assertEquals(expectedPromise.promise, component.onNextButtonClick());
  });

  // Verify the correct text is shown when calibrating from the base.
  test('CalibrationBase', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    assert(component);
    assertEquals(
        loadTimeData.getString('calibrateBaseInstructionsText'),
        strictQuery(instructionsSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertEquals(
        'chrome://shimless-rma/illustrations/base_on_flat_surface.svg',
        strictQuery('img', component.shadowRoot, HTMLImageElement).src);
  });

  // Verify the correct text is shown when calibrating from the lid.
  test('CalibrationLid', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceLidOnFlatSurface);

    assert(component);
    assertEquals(
        loadTimeData.getString('calibrateLidInstructionsText'),
        strictQuery(instructionsSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());
    assertEquals(
        'chrome://shimless-rma/illustrations/lid_on_flat_surface.svg',
        strictQuery('img', component.shadowRoot, HTMLImageElement).src);
  });
});
