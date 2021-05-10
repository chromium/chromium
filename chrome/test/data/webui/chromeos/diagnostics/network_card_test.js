// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_card.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkCardTestSuite() {
  /** @type {?NetworkCardElement} */
  let networkCardElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkCardElement.remove();
    networkCardElement = null;
  });

  function initializeNetworkCard() {
    assertFalse(!!networkCardElement);

    // Add the network info to the DOM.
    networkCardElement = /** @type {!NetworkCardElement} */ (
        document.createElement('network-card'));
    assertTrue(!!networkCardElement);
    document.body.appendChild(networkCardElement);

    return flushTasks();
  }

  test('NetworkCardInitialized', () => {
    return initializeNetworkCard().then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Network');
    });
  });
}