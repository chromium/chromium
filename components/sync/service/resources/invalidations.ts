// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

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

  render(
      getInvalidationsHtml(invalidationCountersArray),
      getRequiredElement('invalidation-counters-table'));
}

function getInvalidationsHtml(data: CountersArrayEntry[]) {
  // clang-format off
  return html`
    <thead>
      <th>Data type</th>
      <th>Count</th>
      <th>Last time</th>
    </thead>
    <tbody>
      ${data.map(item => html`
        <tr>
          <td>${item.type}</td>
          <td>${item.count}</td>
          <td>${item.time}</td>
        </tr>
      `)}
    </tbody>`;
  // clang-format on
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

document.addEventListener('DOMContentLoaded', onLoad);
