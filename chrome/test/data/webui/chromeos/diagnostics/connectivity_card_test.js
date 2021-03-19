// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function connectivityCardTestSuite() {
  /** @type {?ConnectivityCardElement} */
  let connectivityCardElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    connectivityCardElement.remove();
    connectivityCardElement = null;
  });

  function initializeConnectivityCard() {
    assertFalse(!!connectivityCardElement);

    // Add the connectivity card to the DOM.
    connectivityCardElement = /** @type {!ConnectivityCardElement} */ (
        document.createElement('connectivity-card'));
    assertTrue(!!connectivityCardElement);
    document.body.appendChild(connectivityCardElement);

    return flushTasks();
  }

  test('ConnectivityCardPopulated', () => {
    return initializeConnectivityCard().then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.$$('#cardTitle'), 'Connectivity');
    });
  });
}