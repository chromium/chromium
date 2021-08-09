// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

/** @implements {ExtensionControlBrowserProxy} */
export class TestExtensionControlBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'disableExtension',
      'manageExtension',
    ]);
  }

  /** @override */
  disableExtension(extensionId) {
    this.methodCalled('disableExtension', extensionId);
  }

  /** @override */
  manageExtension(extensionId) {
    this.methodCalled('manageExtension', extensionId);
  }
}
