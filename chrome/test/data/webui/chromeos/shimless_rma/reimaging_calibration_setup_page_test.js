// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationSetupPage} from 'chrome://shimless-rma/reimaging_calibration_setup_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationComponentStatus, CalibrationSetupInstruction, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('reimagingCalibrationSetupPageTest', function() {
  /** @type {?ReimagingCalibrationSetupPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @param {!CalibrationSetupInstruction} instructions
   * @return {!Promise}
   */
  function initializeCalibrationPage(instructions) {
    assertFalse(!!component);
    service.setGetCalibrationSetupInstructionsResult(instructions);

    component = /** @type {!ReimagingCalibrationSetupPage} */ (
        document.createElement('reimaging-calibration-setup-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('Initializes', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);
    const instructions = component.shadowRoot.querySelector('#instructions');
    assertFalse(instructions.hidden);
  });

  test('NextButtonTriggersRunCalibrationStep', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);
    let runCalibrationCalls = 0;
    service.runCalibrationStep = () => {
      runCalibrationCalls++;
      return resolver.promise;
    };

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, runCalibrationCalls);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('CalibrationBase', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    assertEquals(
        loadTimeData.getString('calibrateBaseInstructionsText'),
        component.shadowRoot.querySelector('#instructions').textContent.trim());
    assertEquals(
        'chrome://shimless-rma/illustrations/base_on_flat_surface.svg',
        component.shadowRoot.querySelector('img').src);
  });

  test('CalibrationLid', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceLidOnFlatSurface);

    assertEquals(
        loadTimeData.getString('calibrateLidInstructionsText'),
        component.shadowRoot.querySelector('#instructions').textContent.trim());
    assertEquals(
        'chrome://shimless-rma/illustrations/lid_on_flat_surface.svg',
        component.shadowRoot.querySelector('img').src);
  });
});
