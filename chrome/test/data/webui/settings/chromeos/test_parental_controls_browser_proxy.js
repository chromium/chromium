// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestParentalControlsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showAddSupervisionDialog',
      'launchFamilyLinkSettings',
    ]);
  }

  launchFamilyLinkSettings() {
    this.methodCalled('launchFamilyLinkSettings');
  }

  showAddSupervisionDialog() {
    this.methodCalled('showAddSupervisionDialog');
  }
}
