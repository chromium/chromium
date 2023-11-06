// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HatsBrowserProxy, SafeBrowsingSetting, SecurityPageInteraction, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestHatsBrowserProxy extends TestBrowserProxy implements
    HatsBrowserProxy {
  constructor() {
    super([
      'trustSafetyInteractionOccurred',
      'securityPageInteractionOccurred',
    ]);
  }

  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction) {
    this.methodCalled('trustSafetyInteractionOccurred', interaction);
  }

  securityPageInteractionOccurred(
      securityPageInteraction: SecurityPageInteraction,
      safeBrowsingSetting: SafeBrowsingSetting) {
    this.methodCalled(
        'securityPageInteractionOccurred',
        [securityPageInteraction, safeBrowsingSetting]);
  }
}
