// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Tracks metrics calls to verify metric logging in tests. */
export class MetricsTracker {
  private histogramMap_: Map<string, any[]> = new Map();

  count(metricName: string, value?: any): number {
    return this.get_(metricName)
        .filter(v => value === undefined || v === value)
        .length;
  }

  record(metricName: string, value: any) {
    this.get_(metricName).push(value);
  }

  private get_(metricName: string): any[] {
    if (!this.histogramMap_.has(metricName)) {
      this.histogramMap_.set(metricName, []);
    }
    return this.histogramMap_.get(metricName)!;
  }
}

/**
 * Installs interceptors to metrics logging calls and forwards them to the
 * returned |MetricsTracker| object.
 * @return {!MetricsTracker}
 */
export function fakeMetricsPrivate(): MetricsTracker {
  const metrics = new MetricsTracker();
  chrome.metricsPrivate.recordUserAction = (m) => metrics.record(m, 0);
  chrome.metricsPrivate.recordSparseValueWithHashMetricName = (m, v) =>
      metrics.record(m, v);
  chrome.metricsPrivate.recordSparseValueWithPersistentHash = (m, v) =>
      metrics.record(m, v);
  chrome.metricsPrivate.recordBoolean = (m, v) => metrics.record(m, v);
  chrome.metricsPrivate.recordValue = (m, v) => metrics.record(m.metricName, v);
  chrome.metricsPrivate.recordEnumerationValue = (m, v) => metrics.record(m, v);
  chrome.metricsPrivate.recordSmallCount = (m, v) => metrics.record(m, v);
  chrome.metricsPrivate.recordMediumCount = (m, v) => metrics.record(m, v);
  return metrics;
}
