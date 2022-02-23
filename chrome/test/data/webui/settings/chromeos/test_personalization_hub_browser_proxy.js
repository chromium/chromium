// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {TestBrowserProxy} from '../../test_browser_proxy.js';

cr.define('settings', function() {
  /** @implements {settings.PersonalizationHubBrowserProxy} */
  /* #export */ class TestPersonalizationHubBrowserProxy extends
      TestBrowserProxy {
    constructor() {
      super(['openPersonalizationHub']);
    }

    /** @override */
    openPersonalizationHub() {
      this.methodCalled('openPersonalizationHub');
    }
  }

  // #cr_define_end
  return {TestPersonalizationHubBrowserProxy};
});
