// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {PrivacyHubBrowserProxy} */
export class TestPrivacyHubBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getInitialMicrophoneHardwareToggleState',
    ]);
    this.microphoneToggleIsEnabled = false;
  }

  /** override */
  getInitialMicrophoneHardwareToggleState() {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }
}
