// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * A map from data type to number of invalidations received.
 */
const invalidationCountersMap = {};

/**
 * Redraws the counters table with the most recent information.
 */
function refreshInvalidationCountersDisplay() {
  // Transform the counters map into an array.
  const invalidationCountersArray = [];
  Object.keys(invalidationCountersMap).sort().forEach(function(t) {
    invalidationCountersArray.push({
      type: t,
      count: invalidationCountersMap[t].count,
      time: invalidationCountersMap[t].time,
    });
  });

  jstProcess(
      new JsEvalContext({rows: invalidationCountersArray}),
      $('invalidation-counters-table'));
}

/**
 * Appends a string to the textarea log.
 * @param {string} logMessage The string to be appended.
 */
function appendToLog(logMessage) {
  const invalidationsLog = $('invalidations-log');
  invalidationsLog.value +=
      '[' + new Date().getTime() + '] ' + logMessage + '\n';
}

/**
 * Updates the counters for the received types.
 * @param {!Array} types Contains a list of invalidated types.
 */
function onInvalidationReceived(types) {
  const logMessage = 'Received invalidation for ' + types.join(', ');
  appendToLog(logMessage);

  for (const type of types) {
    if (!(type in invalidationCountersMap)) {
      invalidationCountersMap[type] = {count: 0, time: ''};
    }

    ++invalidationCountersMap[type].count;
    invalidationCountersMap[type].time = new Date().toTimeString();
  }

  refreshInvalidationCountersDisplay();
}

function onLoad() {
  addWebUIListener('onInvalidationReceived', onInvalidationReceived);
  refreshInvalidationCountersDisplay();
}

document.addEventListener('DOMContentLoaded', onLoad, false);
