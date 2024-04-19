// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SwitchAccessSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSwitchAccessSubpageBrowserProxy extends TestBrowserProxy
    implements SwitchAccessSubpageBrowserProxy {
  constructor() {
    super([
      'refreshAssignmentsFromPrefs',
      'notifySwitchAccessActionAssignmentPaneActive',
      'notifySwitchAccessActionAssignmentPaneInactive',
      'notifySwitchAccessSetupGuideAttached',
    ]);
  }

  refreshAssignmentsFromPrefs(): void {
    this.methodCalled('refreshAssignmentsFromPrefs');
  }

  notifySwitchAccessActionAssignmentPaneActive(): void {
    this.methodCalled('notifySwitchAccessActionAssignmentPaneActive');
  }

  notifySwitchAccessActionAssignmentPaneInactive(): void {
    this.methodCalled('notifySwitchAccessActionAssignmentPaneInactive');
  }

  notifySwitchAccessSetupGuideAttached(): void {
    this.methodCalled('notifySwitchAccessSetupGuideAttached');
  }
}
