// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';

/**
 * A map from data type to number of invalidations received.
 */
const invalidationCountersMap:
    {[key: string]: {count: number, time: string}} = {};

interface CountersArrayEntry {
  type: string;
  count: number;
  time: string;
}

/**
 * Redraws the counters table with the most recent information.
 */
function refreshInvalidationCountersDisplay() {
  // Transform the counters map into an array.
  const invalidationCountersArray: CountersArrayEntry[] = [];
  Object.keys(invalidationCountersMap).sort().forEach(function(t) {
    invalidationCountersArray.push({
      type: t,
      count: invalidationCountersMap[t]!.count,
      time: invalidationCountersMap[t]!.time,
    });
  });

  const table =
      document.querySelector<HTMLElement>('#invalidation-counters-table');
  assert(table);
  jstProcess(new JsEvalContext({rows: invalidationCountersArray}), table);
}

/**
 * Appends a string to the textarea log.
 * @param logMessage The string to be appended.
 */
function appendToLog(logMessage: string) {
  const invalidationsLog =
      document.querySelector<HTMLTextAreaElement>('#invalidations-log');
  assert(invalidationsLog);
  invalidationsLog.value +=
      '[' + new Date().getTime() + '] ' + logMessage + '\n';
}

/**
 * Updates the counters for the received types.
 * @param types Contains a list of invalidated types.
 */
function onInvalidationReceived(types: string[]) {
  const logMessage = 'Received invalidation for ' + types.join(', ');
  appendToLog(logMessage);

  for (const type of types) {
    if (!(type in invalidationCountersMap)) {
      invalidationCountersMap[type] = {count: 0, time: ''};
    }

    ++invalidationCountersMap[type]!.count;
    invalidationCountersMap[type]!.time = new Date().toTimeString();
  }

  refreshInvalidationCountersDisplay();
}

function onLoad() {
  addWebUiListener('onInvalidationReceived', onInvalidationReceived);
  refreshInvalidationCountersDisplay();
}

document.addEventListener('DOMContentLoaded', onLoad, false);
