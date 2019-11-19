// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('reset_page', function() {
  /** @implements {settings.OsResetBrowserProxy} */
  class TestOsResetBrowserProxy extends TestBrowserProxy {
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

  return {
    TestOsResetBrowserProxy: TestOsResetBrowserProxy,
  };
});
