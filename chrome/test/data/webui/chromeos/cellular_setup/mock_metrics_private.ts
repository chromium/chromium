// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A mock to intercept metric logging calls.
 */
export class MockMetricsPrivate {
  private cellularSetupResultDict_: Map<number, number> = new Map();
  private histogramCounts_: Map<string, number> = new Map();

  recordEnumerationValue(
      histogramName: string, setupFlowResult: number, _enumSize: number): void {
    const entry = this.cellularSetupResultDict_.get(setupFlowResult) || 0;
    this.cellularSetupResultDict_.set(setupFlowResult, entry + 1);
    this.incrementHistogramCount_(histogramName);
  }

  recordLongTime(histogramName: string, _value: number) {
    this.incrementHistogramCount_(histogramName);
  }

  getHistogramEnumValueCount(enumValue: number): number {
    return this.cellularSetupResultDict_.get(enumValue) || 0;
  }

  getHistogramCount(histogramName: string): number {
    return this.histogramCounts_.get(histogramName) || 0;
  }

  private incrementHistogramCount_(histogramName: string): void {
    const entry = this.histogramCounts_.get(histogramName) || 0;
    this.histogramCounts_.set(histogramName, entry + 1);
  }
}
