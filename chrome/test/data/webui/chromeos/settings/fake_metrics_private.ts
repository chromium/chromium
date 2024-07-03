// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const FALSE_COUNT = 0;
const TRUE_COUNT = 1;

type MetricsPrivateApi = typeof chrome.metricsPrivate;
type Histogram = chrome.metricsPrivate.Histogram;
type MetricType = chrome.metricsPrivate.MetricType;
const MetricTypeType = chrome.metricsPrivate.MetricTypeType;

export class FakeMetricsPrivate implements MetricsPrivateApi {
  // Mirroring chrome.metricsPrivate API members.
  /* eslint-disable @typescript-eslint/naming-convention */
  MetricTypeType = MetricTypeType;
  /* eslint-enable @typescript-eslint/naming-convention */

  collectedMetrics: Map<string, {[key: number]: number}>;

  constructor() {
    this.collectedMetrics = new Map();
  }

  recordEnumerationValue(metric: string, value: number, _enumSize: number):
      void {
    const metricEntry = this.collectedMetrics.get(metric) || {};

    if (value in metricEntry) {
      metricEntry[value]! += 1;
    } else {
      metricEntry[value] = 1;
    }
    this.collectedMetrics.set(metric, metricEntry);
  }

  countMetricValue(metric: string, value: number): number {
    const metricEntry = this.collectedMetrics.get(metric);

    if (metricEntry) {
      if (value in metricEntry) {
        return metricEntry[value] as number;
      }
    }
    return 0;
  }

  recordBoolean(metric: string, value: boolean): void {
    const metricEntry = this.collectedMetrics.get(metric) ||
        {[TRUE_COUNT]: 0, [FALSE_COUNT]: 0};
    if (value) {
      metricEntry[TRUE_COUNT]! += 1;
    } else {
      metricEntry[FALSE_COUNT]! += 1;
    }
    this.collectedMetrics.set(metric, metricEntry);
  }

  countBoolean(metric: string, value: boolean): number {
    const metricEntry = this.collectedMetrics.get(metric);

    if (metricEntry) {
      if (value) {
        return metricEntry[TRUE_COUNT] as number;
      } else {
        return metricEntry[FALSE_COUNT] as number;
      }
    } else {
      return 0;
    }
  }

  // The methods below are unimplemented and only added to satisfy the
  // chrome.metricsPrivate interface during TS compilation.

  async getHistogram(): Promise<Histogram> {
    return {sum: 0, buckets: [{min: 0, max: 0, count: 0}]};
  }

  async getIsCrashReportingEnabled(): Promise<boolean> {
    return true;
  }

  async getFieldTrial(): Promise<string> {
    return '';
  }

  async getVariationParams(): Promise<Record<string, string>> {
    return {};
  }

  recordUserAction(_name: string): void {}
  recordPercentage(_metricName: string, _value: number): void {}
  recordCount(_metricName: string, _value: number): void {}
  recordSmallCount(_metricName: string, _value: number): void {}
  recordMediumCount(_metricName: string, _value: number): void {}
  recordTime(_metricName: string, _value: number): void {}
  recordMediumTime(_metricName: string, _value: number): void {}
  recordLongTime(_metricName: string, _value: number): void {}
  recordSparseValueWithHashMetricName(_metricName: string, _value: string):
      void {}
  recordSparseValueWithPersistentHash(_metricName: string, _value: string):
      void {}
  recordSparseValue(_metricName: string, _value: number): void {}
  recordValue(_metric: MetricType, _value: number): void {}
}
