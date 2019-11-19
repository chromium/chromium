// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.sync.types', function() {
  const typeCountersMap = {};

  /**
   * Redraws the counters table taking advantage of the most recent
   * available information.
   *
   * Makes use of typeCountersMap, which is defined in the containing scope.
   */
  function refreshTypeCountersDisplay() {
    const typeCountersArray = [];

    // Transform our map into an array to make jstemplate happy.
    Object.keys(typeCountersMap).sort().forEach(function(t) {
      typeCountersArray.push({
        type: t,
        counters: typeCountersMap[t],
      });
    });

    jstProcess(
        new JsEvalContext({ rows: typeCountersArray }),
        $('type-counters-table'));
  }

  /**
   * Helps to initialize the table by picking up where initTypeCounters() left
   * off.  That function registers this listener and requests that this event
   * be emitted.
   *
   * @param {!Object} e An event containing the list of known sync types.
   */
  function onReceivedListOfTypes(e) {
    const types = e.details.types;
    types.map(function(type) {
      if (!typeCountersMap.hasOwnProperty(type)) {
        typeCountersMap[type] = {};
      }
    });
    chrome.sync.events.removeEventListener(
        'onReceivedListOfTypes',
        onReceivedListOfTypes);
    refreshTypeCountersDisplay();
  }

  /**
   * Callback for receipt of updated per-type counters.
   *
   * @param {!Object} e An event containing an updated counter.
   */
  function onCountersUpdated(e) {
    const details = e.details;

    const modelType = details.modelType;
    const counters = details.counters;

    if (typeCountersMap.hasOwnProperty(modelType)) {
      for (const k in counters) {
        typeCountersMap[modelType][k] = counters[k];
      }
    }
    refreshTypeCountersDisplay();
  }

  /**
   * Initializes state and callbacks for the per-type counters and status UI.
   */
  function initTypeCounters() {
    chrome.sync.events.addEventListener(
        'onCountersUpdated',
        onCountersUpdated);
    chrome.sync.events.addEventListener(
        'onReceivedListOfTypes',
        onReceivedListOfTypes);

    chrome.sync.requestListOfTypes();
    chrome.sync.registerForPerTypeCounters();
  }

  function onLoad() {
    initTypeCounters();
  }

  return {
    onLoad: onLoad
  };
});

document.addEventListener('DOMContentLoaded', chrome.sync.types.onLoad, false);
