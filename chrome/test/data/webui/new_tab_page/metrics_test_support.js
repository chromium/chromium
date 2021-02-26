// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A fake to intercept metrics logging calls and verify how many times they were
 * called.
 */
export class FakeMetricsPrivate {
  constructor() {
    /** @private {!Map<string, !Array>} */
    this.histogramMap_ = new Map();
  }

  /**
   * @param {string} metricName
   * @param {string=} value
   * @return {number}
   */
  count(metricName, value) {
    return this.get_(metricName)
        .filter(v => value === undefined || v === value)
        .length;
  }

  /** @param {string} metricName */
  recordUserAction(metricName) {
    this.get_(metricName).push(0);
  }

  /**
   * @param {string} metricName
   * @param {string} value
   */
  recordSparseHashable(metricName, value) {
    this.get_(metricName).push(value);
  }

  /**
   * @param {string} metricName
   * @param {boolean} value
   */
  recordBoolean(metricName, value) {
    this.get_(metricName).push(value);
  }

  /**
   * @param {string} metricName
   * @return {!Map<string, !Array>}
   * @private
   */
  get_(metricName) {
    if (!this.histogramMap_.has(metricName)) {
      this.histogramMap_.set(metricName, []);
    }
    return this.histogramMap_.get(metricName);
  }
}
