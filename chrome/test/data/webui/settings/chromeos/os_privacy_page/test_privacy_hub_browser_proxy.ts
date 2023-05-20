// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrivacyHubBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacyHubBrowserProxy extends TestBrowserProxy implements
    PrivacyHubBrowserProxy {
  microphoneToggleIsEnabled: boolean;
  constructor() {
    super([
      'getInitialMicrophoneHardwareToggleState',
      'sendLeftOsPrivacyPage',
      'sendOpenedOsPrivacyPage',
    ]);
    this.microphoneToggleIsEnabled = false;
  }

  getInitialMicrophoneHardwareToggleState(): Promise<boolean> {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }

  sendLeftOsPrivacyPage(): void {
    this.methodCalled('sendLeftOsPrivacyPage');
  }

  sendOpenedOsPrivacyPage(): void {
    this.methodCalled('sendOpenedOsPrivacyPage');
  }
}
