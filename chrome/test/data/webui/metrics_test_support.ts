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
  const metricsPrivate =
      (chrome as any).metricsPrivate || ((chrome as any).metricsPrivate = {});
  metricsPrivate.recordUserAction = (m: string) => metrics.record(m, 0);
  metricsPrivate.recordSparseValueWithHashMetricName = (m: string, v: number) =>
      metrics.record(m, v);
  metricsPrivate.recordSparseValueWithPersistentHash = (m: string, v: number) =>
      metrics.record(m, v);
  metricsPrivate.recordBoolean = (m: string, v: boolean) =>
      metrics.record(m, v);
  metricsPrivate.recordValue = (m: {metricName: string}, v: number) =>
      metrics.record(m.metricName, v);
  metricsPrivate.recordEnumerationValue = (m: string, v: number) =>
      metrics.record(m, v);
  metricsPrivate.recordSmallCount = (m: string, v: number) =>
      metrics.record(m, v);
  metricsPrivate.recordMediumCount = (m: string, v: number) =>
      metrics.record(m, v);
  metricsPrivate.recordTime = (m: string, v: number) => metrics.record(m, v);
  metricsPrivate.recordMediumTime = (m: string, v: number) =>
      metrics.record(m, v);

  // Mirror for chrome.histograms callers.
  const histograms =
      (chrome as any).histograms || ((chrome as any).histograms = {});
  histograms.recordUserAction = (m: string) => metrics.record(m, 0);
  histograms.recordBoolean = (m: string, v: boolean) => metrics.record(m, v);
  histograms.recordPercentage = (m: string, v: number) => metrics.record(m, v);
  histograms.recordSmallCount = (m: string, v: number) => metrics.record(m, v);
  histograms.recordMediumCount = (m: string, v: number) => metrics.record(m, v);
  histograms.recordCount = (m: string, v: number) => metrics.record(m, v);
  histograms.recordTime = (m: string, v: number) => metrics.record(m, v);
  histograms.recordMediumTime = (m: string, v: number) => metrics.record(m, v);
  histograms.recordLongTime = (m: string, v: number) => metrics.record(m, v);
  histograms.recordValue = (metric: {metricName: string}, v: number) =>
      metrics.record(metric.metricName, v);
  histograms.recordEnumerationValue = (m: string, v: number) =>
      metrics.record(m, v);
  histograms.recordSparseValue = (m: string, v: number) => metrics.record(m, v);
  return metrics;
}
