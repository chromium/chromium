// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {OsResetBrowserProxy} */
export class TestOsResetBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'onPowerwashDialogShow',
    ]);
  }

  /** @override */
  onPowerwashDialogShow() {
    this.methodCalled('onPowerwashDialogShow');
  }
}
