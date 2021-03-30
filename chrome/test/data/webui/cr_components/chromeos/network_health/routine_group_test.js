// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://connectivity-diagnostics/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics_mojo.m.js'
// #import {Routine, Icons} from 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics_types.m.js'
// #import 'chrome://resources/cr_components/chromeos/network_health/routine_group.m.js'
// #import {assertTrue, assertFalse, assertEquals} from '../../../chai_assert.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

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
      result: {
        verdict: chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem
      },
      ariaDescription: '',
    },
    {
      name: 'NetworkDiagnosticsSignalStrength',
      running: false,
      resultMsg: 'Passed',
      group: 0,
      type: 1,
      result: {
        verdict: chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem
      },
      ariaDescription: '',
    }
  ];
}

/**
 * Removes any prefixed URL from a icon image path
 * @param {string}
 * @return {string}
 * @private
 */
function getIconFromSrc(src) {
  const values = src.split('/');
  return values[values.length - 1];
}

/**
 * Test suite for the Routine Group element.
 */
suite('RoutineGroupTest', function routineGroupTest() {
  /** @type {?RoutineGroupElement} */
  let routineGroup = null;

  setup(() => {
    document.body.innerHTML = '';
    routineGroup = /** @type {!RoutineGroupElement} */ (
        document.createElement('routine-group'));
    routineGroup.name = 'Group';
    routineGroup.expanded = false;
    document.body.appendChild(routineGroup);
    Polymer.dom.flush();
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
    Polymer.dom.flush();
  }

  /**
   * Clicks the routine group container to toggle the expanded state.
   * @param {boolean} expanded
   */
  function clickRoutineGroup() {
    const container = routineGroup.$$('network-health-container');
    assertTrue(!!container);
    container.click();
    Polymer.dom.flush();
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
    let routines = createRoutines();
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
    let routines = createRoutines();
    for (let routine of routines) {
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
    Polymer.dom.flush();

    checkResult(Icons.TEST_PASSED);
    clickRoutineGroup();
  });

  /**
   * Test when all routines are complete and one has failed.
   */
  test('FailedOne', () => {
    let routines = createRoutines();
    routines[0].resultMsg = 'Failed';
    routines[0].result = {
      'verdict': chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem
    };
    setRoutines(routines);
    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete and one did not run.
   */
  test('NotRunOne', () => {
    let routines = createRoutines();
    routines[0].resultMsg = 'Not Run';
    routines[0].result = {
      'verdict': chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun
    };
    setRoutines(routines);
    checkResult(Icons.TEST_NOT_RUN);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete. One routine failed and one did not run.
   */
  test('NotRunAndFailed', () => {
    let routines = createRoutines();
    routines[0].resultMsg = 'Not Run';
    routines[0].result = {
      'verdict': chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun
    };
    routines[1].resultMsg = 'Failed';
    routines[1].result = {
      'verdict': chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem
    };
    setRoutines(routines);
    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });
});
