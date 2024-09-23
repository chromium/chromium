// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://connectivity-diagnostics/strings.m.js';
import 'chrome://resources/ash/common/network_health/routine_group.js';

import type {Routine, RoutineResponse} from 'chrome://resources/ash/common/network_health/network_diagnostics_types.js';
import {Icons} from 'chrome://resources/ash/common/network_health/network_diagnostics_types.js';
import type {RoutineGroupElement} from 'chrome://resources/ash/common/network_health/routine_group.js';
import {RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createResult, getIconFromSrc} from './network_health_test_utils.js';

/**
 * Test suite for the Routine Group element.
 */
suite('RoutineGroupTest', () => {
  let routineGroup: RoutineGroupElement;

  /**
   * Creates baseline routines.
   */
  function createRoutines(): Routine[] {
    return [
      {
        name: 'NetworkDiagnosticsLanConnectivity',
        running: false,
        resultMsg: 'Passed',
        group: 0,
        type: 0,
        result: createResult(RoutineVerdict.kNoProblem),
        ariaDescription: '',
        func(): Promise<RoutineResponse> {
          return Promise.resolve({
            result: createResult(RoutineVerdict.kNoProblem),
          });
        },
      },
      {
        name: 'NetworkDiagnosticsSignalStrength',
        running: false,
        resultMsg: 'Passed',
        group: 0,
        type: 1,
        result: createResult(RoutineVerdict.kNoProblem),
        ariaDescription: '',
        func(): Promise<RoutineResponse> {
          return Promise.resolve({
            result: createResult(RoutineVerdict.kNoProblem),
          });
        },
      },
    ];
  }

  setup(() => {
    routineGroup = document.createElement('routine-group');
    routineGroup.name = 'Group';
    routineGroup.expanded = false;
    document.body.appendChild(routineGroup);
    flush();
  });

  teardown(() => {
    routineGroup.remove();
  });

  /**
   * Takes the provided routines and passes them to the RoutineGroupElement,
   * then flushes the Polymes DOM.
   */
  function setRoutines(routines: Routine[]): void {
    routineGroup.routines = routines;
    flush();
  }

  /**
   * Clicks the routine group container to toggle the expanded state.
   */
  function clickRoutineGroup(): void {
    const container =
        routineGroup.shadowRoot!.querySelector('network-health-container');
    assertTrue(!!container);
    container.click();
    flush();
  }

  /**
   * Check that the spinner is visible and the group icon is not.
   */
  function checkRunning(): void {
    const spinner =
        routineGroup.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertFalse(spinner.hidden);
    const icon = routineGroup.shadowRoot!.querySelector<HTMLImageElement>(
        '.routine-icon');
    assertTrue(!!icon);
    assertTrue(icon.hidden);
  }

  /**
   * Check that the spinner is hidden and the group icon is visible and set to
   * `iconResult`.
   */
  function checkResult(iconResult: string): void {
    const spinner =
        routineGroup.shadowRoot!.querySelector('paper-spinner-lite');
    assertFalse(!!spinner);
    const icon = routineGroup.shadowRoot!.querySelector<HTMLImageElement>(
        '.routine-icon');
    assertTrue(!!icon);
    assertFalse(icon.hidden);
    assertEquals(getIconFromSrc(icon.src), iconResult);
  }

  /**
   * Test using one running routine.
   */
  test('RunningOne', () => {
    const routines = createRoutines();
    assertFalse(!routines.length);
    routines[0]!.running = true;
    routines[0]!.result = null;
    routines[0]!.resultMsg = '';
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
    routines[0]!.resultMsg = 'Failed';
    routines[0]!.result = createResult(RoutineVerdict.kProblem);
    setRoutines(routines);

    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete and one did not run.
   */
  test('NotRunOne', () => {
    const routines = createRoutines();
    routines[0]!.resultMsg = 'Not Run';
    routines[0]!.result = createResult(RoutineVerdict.kNotRun);
    setRoutines(routines);

    checkResult(Icons.TEST_NOT_RUN);
    clickRoutineGroup();
  });

  /**
   * Test when routines are complete. One routine failed and one did not run.
   */
  test('NotRunAndFailed', () => {
    const routines = createRoutines();
    routines[0]!.resultMsg = 'Not Run';
    routines[0]!.result = createResult(RoutineVerdict.kNotRun);
    routines[1]!.resultMsg = 'Failed';
    routines[1]!.result = createResult(RoutineVerdict.kProblem);
    setRoutines(routines);

    checkResult(Icons.TEST_FAILED);
    clickRoutineGroup();
  });
});
