// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {OsA11yPageBrowserProxy} */
export class TestOsA11yPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'a11yPageReady',
      'confirmA11yImageLabels',
    ]);
  }

  a11yPageReady() {
    this.methodCalled('a11yPageReady');
  }

  confirmA11yImageLabels() {
    this.methodCalled('confirmA11yImageLabels');
  }
}
