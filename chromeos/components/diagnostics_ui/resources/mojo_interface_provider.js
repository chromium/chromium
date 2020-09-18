// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {SystemDataProviderInterface} from './diagnostics_types.js'

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?SystemDataProviderInterface}
 */
let systemDataProvider = null;

/**
 * @param {!SystemDataProviderInterface} testProvider
 */
export function setSystemDataProviderForTesting(testProvider) {
  systemDataProvider = testProvider;
}

/**
 * @return {!SystemDataProviderInterface}
 */
export function getSystemDataProvider() {
  // TODO(zentaro): Instantiate a real mojo interface here.
  assert(!!systemDataProvider);

  return systemDataProvider;
}
