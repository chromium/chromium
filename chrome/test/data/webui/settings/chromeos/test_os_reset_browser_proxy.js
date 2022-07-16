// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {TestBrowserProxy} from '../../test_browser_proxy.js';

cr.define('reset_page', function() {
  /** @implements {settings.OsResetBrowserProxy} */
  /* #export */ class TestOsResetBrowserProxy extends TestBrowserProxy {
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

  // #cr_define_end
  return {
    TestOsResetBrowserProxy: TestOsResetBrowserProxy,
  };
});
