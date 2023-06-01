// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const FALSE_COUNT: number = 0;
const TRUE_COUNT: number = 1;

export class FakeMetricsPrivate {
  collectedMetrics: Map<string, {[key: number]: number}>;
  constructor() {
    this.collectedMetrics = new Map();
  }

  recordSparseValueWithPersistentHash(_metricName: string, _value: string):
      void {}

  recordEnumerationValue(metric: string, value: number, _enumSize: number):
      void {
    const metricEntry = this.collectedMetrics.get(metric) || {};

    if (value in metricEntry) {
      metricEntry[value] += 1;
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
      metricEntry[TRUE_COUNT] += 1;
    } else {
      metricEntry[FALSE_COUNT] += 1;
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
}
