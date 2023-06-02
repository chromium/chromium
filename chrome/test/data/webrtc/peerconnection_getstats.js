/**
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// Public interface to tests.

/**
 * Gets the result of the promise-based |RTCPeerConnection.getStats| as a
 * dictionary of RTCStats-dictionaries.
 *
 * Returns "ok-" followed by a JSON-stringified dictionary of dictionaries to
 * the test.
 */
function getStatsReportDictionary() {
  return peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new Error('report is null or empty.');
      let reportDictionary = {};
      for (let stats of report.values()) {
        reportDictionary[stats.id] = stats;
      }
      return logAndReturn('ok-' + JSON.stringify(reportDictionary));
    },
    function(e) {
      throw new Error('Promise was rejected: ' + e);
    });
}

/**
 * Measures the performance of the promise-based |RTCPeerConnection.getStats|
 * and returns the time it took in milliseconds as a double
 * (DOMHighResTimeStamp, accurate to one thousandth of a millisecond).
 * Verifies that all stats types of the allowlist were contained in the result.
 *
 * Returns "ok-" followed by a double on success.
 */
function measureGetStatsPerformance() {
  let t0 = performance.now();
  return peerConnection_().getStats()
    .then(function(report) {
      let t1 = performance.now();
      return logAndReturn('ok-' + (t1 - t0));
    },
    function(e) {
      throw new Error('Promise was rejected: ' + e);
    });
}

// Internals.

/** Gets a set of all mandatory stats types. */
function mandatoryStatsTypes() {
  const mandatoryTypes = new Set();
  for (let allowlistedStats of gStatsAllowlist.values()) {
    if (allowlistedStats.presence() == Presence.MANDATORY)
      mandatoryTypes.add(allowlistedStats.type());
  }
  return mandatoryTypes;
}

/**
 * Checks if |stats| correctly maps to a a allowlisted RTCStats-derived
 * dictionary, throwing an error if it doesn't. See |gStatsAllowlist|.
 *
 * The "RTCStats.type" must map to a known dictionary description. Every member
 * is optional, but if present it must be present in the allowlisted dictionary
 * description and its type must match.
 * @private
 */
function verifyStatsIsAllowlisted_(stats) {
  if (stats == null)
    throw new Error('stats is null or undefined: ' + stats);
  if (typeof(stats.id) !== 'string')
    throw new Error('stats.id is not a string:' + stats.id);
  if (typeof(stats.timestamp) !== 'number' || !isFinite(stats.timestamp) ||
      stats.timestamp <= 0) {
    throw new Error('stats.timestamp is not a positive finite number: ' +
        stats.timestamp);
  }
  if (typeof(stats.type) !== 'string')
    throw new Error('stats.type is not a string: ' + stats.type);
  let allowlistedStats = gStatsAllowlist.get(stats.type);
  if (allowlistedStats == null)
    throw new Error('stats.type is not a allowlisted type: ' + stats.type);
  allowlistedStats = allowlistedStats.stats();
  for (let propertyName in stats) {
    if (propertyName === 'id' || propertyName === 'timestamp' ||
        propertyName === 'type') {
      continue;
    }
    if (!allowlistedStats.hasOwnProperty(propertyName)) {
      throw new Error(
          stats.type + '.' + propertyName + ' is not a allowlisted ' +
          'member: ' + stats[propertyName]);
    }
    if (allowlistedStats[propertyName] === 'any')
      continue;
    if (!allowlistedStats[propertyName].startsWith('sequence_')) {
      if (typeof(stats[propertyName]) !== allowlistedStats[propertyName]) {
        throw new Error('stats.' + propertyName + ' should have a different ' +
            'type according to the allowlist: ' + stats[propertyName] + ' vs ' +
            allowlistedStats[propertyName]);
      }
    } else {
      if (!Array.isArray(stats[propertyName])) {
        throw new Error('stats.' + propertyName + ' should have a different ' +
            'type according to the allowlist (should be an array): ' +
            JSON.stringify(stats[propertyName]) + ' vs ' +
            allowlistedStats[propertyName]);
      }
      let elementType = allowlistedStats[propertyName].substring(9);
      for (let element in stats[propertyName]) {
        if (typeof(element) !== elementType) {
          throw new Error('stats.' + propertyName +
              ' should have a different ' +
              'type according to the allowlist (an element of the array has ' +
              'the incorrect type): ' + JSON.stringify(stats[propertyName]) +
              ' vs ' + allowlistedStats[propertyName]);
        }
      }
    }
  }
}
