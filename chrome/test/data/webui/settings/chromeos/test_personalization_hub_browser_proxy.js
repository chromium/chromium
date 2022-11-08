// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {PersonalizationHubBrowserProxy} */
export class TestPersonalizationHubBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['openPersonalizationHub']);
  }

  /** @override */
  openPersonalizationHub() {
    this.methodCalled('openPersonalizationHub');
  }
}
