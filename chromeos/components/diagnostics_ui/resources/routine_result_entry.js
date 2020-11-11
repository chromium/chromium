// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineName, RoutineResult, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';

/**
 * Resolves an enum value to the string name. This is used temporarily to
 * provide a human readable string until the final mapping of enum values to
 * localized strings is finalized.
 * TODO(zentaro): Remove this function when strings are finalized.
 * @param {!Object} enumType
 * @param {number} enumValue
 * @return {string}
 */
function lookupEnumValueName(enumType, enumValue) {
  for (const [key, value] of Object.entries(enumType)) {
    if (value === enumValue) {
      return key;
    }
  }

  // Values should always be found in the enum.
  assert(false);
  return '';
}

/**
 * @fileoverview
 * 'routine-result-entry' shows the status of a single test routine.
 */
Polymer({
  is: 'routine-result-entry',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!ResultStatusItem} */
    item: {
      type: Object,
    },

    /** @private */
    routineName_: {
      type: String,
      computed: 'getRoutineName_(item.routine)',
    },

    /** @private */
    status_: {
      type: String,
      computed: 'getRoutineStatus_(item.progress, item.result)',
    },
  },

  /**
   * Get the string name for the routine.
   * TODO(zentaro): Replace with a mapping to localized string when they are
   * finalized.
   * @param {!RoutineName} routine
   * @return {string}
   */
  getRoutineName_(routine) {
    return lookupEnumValueName(RoutineName, routine);
  },

  /**
   * Get the status for the routine. This is a combination of progress and
   * result.
   * TODO(zentaro): Replace with a mapping to localized string when they are
   * finalized.
   * @param {!ExecutionProgress} progress
   * @param {!RoutineResult} result
   * @return {string}
   */
  getRoutineStatus_(progress, result) {
    if (progress === ExecutionProgress.kNotStarted) {
      return '';
    }

    if (progress === ExecutionProgress.kRunning) {
      return lookupEnumValueName(ExecutionProgress, ExecutionProgress.kRunning);
    }

    return lookupEnumValueName(StandardRoutineResult, result.simpleResult);
  },

  /** @override */
  created() {},
});
