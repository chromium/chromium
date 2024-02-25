// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MetricsReporter} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';

export class MockedMetricsReporter implements MetricsReporter {
  mark(_name: string): void {}

  measure(_startMark: string, _endMark?: string): Promise<bigint> {
    return Promise.resolve(0n);
  }

  hasMark(_name: string): Promise<boolean> {
    return Promise.resolve(false);
  }

  hasLocalMark(_name: string): boolean {
    return false;
  }

  clearMark(_name: string): void {}

  umaReportTime(_histogram: string, _time: bigint): void {}
}