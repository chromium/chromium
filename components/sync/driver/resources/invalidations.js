// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.sync.invalidations', function() {
  'use strict';

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
   * @param {Event} e Contains a list of invalidated types.
   */
  function onInvalidationReceived(e) {
    const logMessage = 'Received invalidation for ' + e.details.join(', ');
    appendToLog(logMessage);

    for (const type of e.details) {
      if (!(type in invalidationCountersMap)) {
        invalidationCountersMap[type] = {count: 0, time: ''};
      }

      ++invalidationCountersMap[type].count;
      invalidationCountersMap[type].time = new Date().toTimeString();
    }

    refreshInvalidationCountersDisplay();
  }

  function onLoad() {
    chrome.sync.events.addEventListener(
        'onInvalidationReceived', onInvalidationReceived);
    refreshInvalidationCountersDisplay();
  }

  return {onLoad: onLoad};
});

document.addEventListener(
    'DOMContentLoaded', chrome.sync.invalidations.onLoad, false);
