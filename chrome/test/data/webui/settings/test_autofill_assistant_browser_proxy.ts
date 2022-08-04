// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutofillAssistantBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAutofillAssistantBrowserProxy extends TestBrowserProxy
    implements AutofillAssistantBrowserProxy {
  constructor() {
    super([
      'promptForConsent',
      'revokeConsent',
    ]);
  }

  promptForConsent() {
    this.methodCalled('promptForConsent');
    return Promise.resolve(true);
  }

  revokeConsent(dialogElements: string[]) {
    this.methodCalled('revokeConsent', dialogElements);
  }
}
