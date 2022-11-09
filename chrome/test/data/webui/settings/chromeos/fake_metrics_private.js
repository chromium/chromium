// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class FakeMetricsPrivate {
  constructor() {
    this.collectedMetrics = new Map();
  }

  recordSparseValueWithPersistentHash(metricName, value) {}

  recordEnumerationValue(metric, value, enumSize) {
    const metricEntry = this.collectedMetrics.get(metric) || {};

    if (value in metricEntry) {
      metricEntry[value] = metricEntry[value] + 1;
    } else {
      metricEntry[value] = 1;
    }
    this.collectedMetrics[metric] = metricEntry;
  }

  countMetricValue(metric, value) {
    const metricEntry = this.collectedMetrics[metric];

    if (metricEntry) {
      if (value in metricEntry) {
        return metricEntry[value];
      }
    }
    return 0;
  }

  recordBoolean(metric, value) {
    const metricEntry =
        this.collectedMetrics.get(metric) || {trueCnt: 0, falseCnt: 0};
    if (value) {
      metricEntry.trueCnt = metricEntry.trueCnt + 1;
    } else {
      metricEntry.falseCnt = metricEntry.falseCnt + 1;
    }
    this.collectedMetrics.set(metric, metricEntry);
  }

  countBoolean(metric, value) {
    const metricEntry = this.collectedMetrics.get(metric);

    if (metricEntry) {
      if (value) {
        return metricEntry.trueCnt;
      } else {
        return metricEntry.falseCnt;
      }
    } else {
      return 0;
    }
  }
}
