// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://connectivity-diagnostics/strings.m.js';
import 'chrome://resources/ash/common/network_health/routine_group.js';

import {Icons, Routine} from 'chrome://resources/ash/common/network_health/network_diagnostics_types.js';
import {RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../../chromeos/chai_assert.js';

import {createResult, getIconFromSrc} from './network_health_test_utils.js';

/**
 * Creates baseline routines.
 * @return {!Array<!Routine>}
 * @private
 */
function createRoutines() {
  return [
    {
      name: 'NetworkDiagnosticsLanConnectivity',
      running: false,
      resultMsg: 'Passed',
      group: 0,
      type: 0,
      result: createResult(RoutineVerdict.kNoProblem),
      ariaDescription: '',
    },
    {
      name: 'NetworkDiagnosticsSignalStrength',
      running: false,
      resultMsg: 'Passed',
      group: 0,
      type: 1,
      result: createResult(RoutineVerdict.kNoProblem),
      ariaDescription: '',
    },
  ];
}

/**
 * Test suite for the Routine Group element.
 */
suite('RoutineGroupTest', function routineGroupTest() {
  /** @type {?RoutineGroupElement} */
  let routineGroup = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    routineGroup = /** @type {!RoutineGroupElement} */ (
        document.createElement('routine-group'));
    routineGroup.name = 'Group';
    routineGroup.expanded = false;
    document.body.appendChild(routineGroup);
    flush();
  });

  teardown(function() {
    routineGroup.remove();
    routineGroup = null;
  });

  /**
   * Takes the provided routines and passes them to the RoutineGroupElement,
   * then flushes the Polymes DOM.
   * @param {!Array<!Routine>} routines
   */
  function setRoutines(routines) {
    routineGroup.routines = routines;
    flush();
  }

  /**
   * Clicks the routine group container to toggle the expanded state.
   */
  function clickRoutineGroup() {
    const container = routineGroup.$$('network-health-container');
    assertTrue(!!container);
    container.click();
    flush();
  }

  /**
   * Check that the spinner is visible and the group icon is not.
   */
  function checkRunning() {
    const spinner = routineGroup.$$('paper-spinner-lite');
    assertTrue(!!spinner);
    assertFalse(spinner.hidden);
    const icon = routineGroup.$$('.routine-icon');
    assertTrue(!!icon);
    assertTrue(icon.hidden);
  }

  /**
   * Check that the spinner is hidden and the group icon is visible and set to
   * `iconResult`.
   * @param {string} iconResult
   */
  function checkResult(iconResult) {
    const spinner = routineGroup.$$('paper-spinner-lite');
    assertFalse(!!spinner);
    const icon = routineGroup.$$('.routine-icon');
    assertTrue(!!icon);
    assertFalse(icon.hidden);
    assertEquals(getIconFromSrc(icon.src), iconResult);
  }

  /**
   * Test using one running routine.
   */
  test('RunningOne', () => {
    const routines = createRoutines();
    routines[0].running = true;
    routines[0].result = null;
    routines[0].resultMsg = '';
    setRoutines(routines);

    checkRunning();
    clickRoutineGroup();
  });

  /**
   * Test when all routines are running.
   */
  test('RunningAll', () => {
    const routines = createRoutines();
    for (const routine of routines) {
      routine.running = true;
      routine.result = null;
      routine.resultMsg = '';
    }
    setRoutines(routines);
    checkRunning();
    clickRoutineGroup();
  });

  /**
   * Test when all routines are complete.
   */
  test('RunningNone', () => {
    routineGroup.routines = createRoutines();
    flush();

    checkResult(Icons.TEST_PASSED);
    clickRoutineGroup();
  });

  /**
   * Test when all routines are complete and one has failed.
   */
  test('FailedOne', () => {
    const routines = createRoutines();
    routines[0].resultMsg = 'Failed';
    routines[0].result = createResult(RoutineVerdict.kProblem);
    setRoutines(routines);
    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete and one did not run.
   */
  test('NotRunOne', () => {
    const routines = createRoutines();
    routines[0].resultMsg = 'Not Run';
    routines[0].result = createResult(RoutineVerdict.kNotRun);
    setRoutines(routines);
    checkResult(Icons.TEST_NOT_RUN);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete. One routine failed and one did not run.
   */
  test('NotRunAndFailed', () => {
    const routines = createRoutines();
    routines[0].resultMsg = 'Not Run';
    routines[0].result = createResult(RoutineVerdict.kNotRun);
    routines[1].resultMsg = 'Failed';
    routines[1].result = createResult(RoutineVerdict.kProblem);
    setRoutines(routines);
    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });
});
