// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './routine_result_entry.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineType} from './diagnostics_types.js';
import {ResultStatusItem} from './routine_list_executor.js'

/**
 * @fileoverview
 * 'routine-result-list' shows a list of routine result entries.
 */
Polymer({
  is: 'routine-result-list',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {!Array<!ResultStatusItem>} */
    results_: {
      type: Array,
      value: () => [],
    },

    /** @type {boolean} */
    hidden: {
      type: Boolean,
      value: false,
    }
  },

  /**
   * Resets the list and creates a new list with all routines in the unstarted
   * state. Called by the parent RoutineResultSection when the user starts
   * a test run.
   * @param {!Array<!RoutineType>} routines
   */
  initializeTestRun(routines) {
    this.clearRoutines();
    routines.forEach((routine) => {
      this.addRoutine_(routine);
    });
  },

  /**
   * Removes all the routines from the list.
   */
  clearRoutines() {
    this.splice('results_', 0, this.results_.length);
  },

  /**
   * Add a new unstarted routine to the end of the list.
   * @param {!RoutineType} routine
   * @private
   */
  addRoutine_(routine) {
    this.push('results_', new ResultStatusItem(routine));
  },

  /**
   * Updates the routine's status in the results_ list.
   * @param {number} index
   * @param {!ResultStatusItem} status
   * @private
   */
  updateRoutineStatus_(index, status) {
    assert(index < this.results_.length);
    this.splice('results_', index, 1, status);
  },

  /**
   * Receives the callback from RoutineListExecutor whenever the status of a
   * routine changed.
   * @param {!ResultStatusItem} status
   */
  onStatusUpdate(status) {
    assert(this.results_.length > 0);
    this.results_.forEach((result, index) => {
      if (result.routine == status.routine) {
        this.updateRoutineStatus_(index, status);
        return;
      }
    });
  },

  /** @override */
  created() {},
});
