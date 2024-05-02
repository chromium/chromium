// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BatterySaverModeState, MemorySaverModeExceptionListAction, MemorySaverModeState, PerformanceMetricsProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPerformanceMetricsProxy extends TestBrowserProxy implements
    PerformanceMetricsProxy {
  constructor() {
    super([
      'recordBatterySaverModeChanged',
      'recordMemorySaverModeChanged',
      'recordDiscardRingTreatmentEnabledChanged',
      'recordExceptionListAction',
    ]);
  }

  recordBatterySaverModeChanged(state: BatterySaverModeState) {
    this.methodCalled('recordBatterySaverModeChanged', state);
  }

  recordMemorySaverModeChanged(state: MemorySaverModeState) {
    this.methodCalled('recordMemorySaverModeChanged', state);
  }

  recordDiscardRingTreatmentEnabledChanged(enabled: boolean) {
    this.methodCalled('recordDiscardRingTreatmentEnabledChanged', enabled);
  }

  recordExceptionListAction(action: MemorySaverModeExceptionListAction) {
    this.methodCalled('recordExceptionListAction', action);
  }
}