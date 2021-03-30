// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Tracks metrics calls to verify metric logging in tests. */
export class MetricsTracker {
  constructor() {
    /** @private {!Map<string, !Array<*>>} */
    this.histogramMap_ = new Map();
  }

  /**
   * @param {string} metricName
   * @param {*=} value
   * @return {number}
   */
  count(metricName, value) {
    return this.get_(metricName)
        .filter(v => value === undefined || v === value)
        .length;
  }

  /**
   * @param {string} metricName
   * @param {*} value
   */
  record(metricName, value) {
    this.get_(metricName).push(value);
  }

  /**
   * @param {string} metricName
   * @return {!Array<*>}
   * @private
   */
  get_(metricName) {
    if (!this.histogramMap_.has(metricName)) {
      this.histogramMap_.set(metricName, []);
    }
    return this.histogramMap_.get(metricName);
  }
}

/**
 * Installs interceptors to metrics logging calls and forwards them to the
 * returned |MetricsTracker| object.
 * @return {!MetricsTracker}
 */
export function fakeMetricsPrivate() {
  const metrics = new MetricsTracker();
  chrome.metricsPrivate.recordUserAction = (m) => metrics.record(m, 0);
  chrome.metricsPrivate.recordSparseHashable = (m, v) => metrics.record(m, v);
  chrome.metricsPrivate.recordBoolean = (m, v) => metrics.record(m, v);
  chrome.metricsPrivate.recordValue = (m, v) => metrics.record(m.metricName, v);
  return metrics;
}
