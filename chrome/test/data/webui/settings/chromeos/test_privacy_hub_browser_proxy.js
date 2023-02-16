// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {PrivacyHubBrowserProxy} */
export class TestPrivacyHubBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getInitialMicrophoneHardwareToggleState',
      'sendLeftOsPrivacyPage',
      'sendOpenedOsPrivacyPage',
    ]);
    this.microphoneToggleIsEnabled = false;
    this.sendLeftOsPrivacyPageCalled = 0;
    this.sendOpenedOsPrivacyPageCalled = 0;
  }

  /** override */
  getInitialMicrophoneHardwareToggleState() {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }

  sendLeftOsPrivacyPage() {
    this.sendLeftOsPrivacyPageCalled = this.sendLeftOsPrivacyPageCalled + 1;
  }

  sendOpenedOsPrivacyPage() {
    this.sendOpenedOsPrivacyPageCalled = this.sendOpenedOsPrivacyPageCalled + 1;
  }
}
