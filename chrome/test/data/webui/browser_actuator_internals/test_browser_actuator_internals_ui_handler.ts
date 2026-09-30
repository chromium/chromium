// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserActuatorInternalsUIInterface, SessionSummary} from 'chrome://browser-actuator-internals/browser_actuator_internals.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBrowserActuatorInternalsUiHandler extends TestBrowserProxy
    implements BrowserActuatorInternalsUIInterface {
  private sessions_: SessionSummary[] = [];

  constructor() {
    super([
      'getSessionHistory',
    ]);
  }

  setSessions(sessions: SessionSummary[]) {
    this.sessions_ = sessions;
  }

  getSessionHistory(): Promise<{sessions: SessionSummary[]}> {
    this.methodCalled('getSessionHistory');
    return Promise.resolve({sessions: this.sessions_});
  }
}
