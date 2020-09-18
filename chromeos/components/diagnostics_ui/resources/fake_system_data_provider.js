// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SystemInfo} from './diagnostics_types.js';
import {FakeMethodResolver} from './fake_method_resolver.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemDataProvider mojo interface.
 */

export class FakeSystemDataProvider {
  constructor() {
    /** @private {!FakeMethodResolver} */
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getSystemInfo');
  }

  /**
   * @return {!Promise<!SystemInfo>}
   */
  getSystemInfo() {
    return this.methods_.resolveMethod('getSystemInfo');
  }

  /**
   * Sets the value that will be returned when calling getSystemInfo().
   * @param {!SystemInfo} systemInfo
   */
  setFakeSystemInfo(systemInfo) {
    this.methods_.setResult('getSystemInfo', systemInfo);
  }
}
