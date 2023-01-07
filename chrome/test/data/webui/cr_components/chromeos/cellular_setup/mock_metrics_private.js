// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A mock to intercept metric logging calls.
 */
export class MockMetricsPrivate {
  constructor() {
    this.cellularSetupResultDict_ = {};
    this.histogramCounts_ = {};
  }

  recordEnumerationValue(histogramName, setupFlowResult, enumSize) {
    if (setupFlowResult in this.cellularSetupResultDict_) {
      this.cellularSetupResultDict_[setupFlowResult]++;
    } else {
      this.cellularSetupResultDict_[setupFlowResult] = 1;
    }
    this.incrementHistogramCount_(histogramName);
  }

  recordLongTime(histogramName, value) {
    this.incrementHistogramCount_(histogramName);
  }

  getHistogramEnumValueCount(enumValue) {
    if (enumValue in this.cellularSetupResultDict_) {
      return this.cellularSetupResultDict_[enumValue];
    }
    return 0;
  }

  getHistogramCount(histogramName) {
    if (histogramName in this.histogramCounts_) {
      return this.histogramCounts_[histogramName];
    }
    return 0;
  }

  incrementHistogramCount_(histogramName) {
    if (histogramName in this.histogramCounts_) {
      this.histogramCounts_[histogramName]++;
    } else {
      this.histogramCounts_[histogramName] = 1;
    }
  }
}
