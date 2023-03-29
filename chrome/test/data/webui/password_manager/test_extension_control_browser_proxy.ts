// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionControlBrowserProxy} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestExtensionControlBrowserProxy extends TestBrowserProxy
    implements ExtensionControlBrowserProxy {
  constructor() {
    super([
      'disableExtension',
    ]);
  }

  disableExtension(extensionId: string) {
    this.methodCalled('disableExtension', extensionId);
  }
}
