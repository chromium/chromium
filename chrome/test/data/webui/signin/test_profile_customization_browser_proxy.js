// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProfileCustomizationBrowserProxy} from 'chrome://profile-customization/profile_customization_browser_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {ProfileCustomizationBrowserProxy} */
export class TestProfileCustomizationBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['done']);
  }

  /** @override */
  done() {
    this.methodCalled('done');
  }
}
