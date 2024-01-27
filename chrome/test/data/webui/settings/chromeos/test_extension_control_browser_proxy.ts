// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionControlBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestExtensionControlBrowserProxy extends TestBrowserProxy
    implements ExtensionControlBrowserProxy {
  constructor() {
    super([
      'disableExtension',
      'manageExtension',
    ]);
  }

  disableExtension(extensionId: string) {
    this.methodCalled('disableExtension', extensionId);
  }

  manageExtension(extensionId: string) {
    this.methodCalled('manageExtension', extensionId);
  }
}
