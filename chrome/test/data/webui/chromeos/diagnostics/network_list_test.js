// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_list.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkListTestSuite() {
  /** @type {?NetworkListElement} */
  let networkListElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkListElement.remove();
    networkListElement = null;
  });

  function initializeNetworkList() {
    assertFalse(!!networkListElement);

    // Add the network list to the DOM.
    networkListElement = /** @type {!NetworkListElement} */ (
        document.createElement('network-list'));
    assertTrue(!!networkListElement);
    document.body.appendChild(networkListElement);

    return flushTasks();
  }

  /**
   * Returns the connectivity-card element.
   * @return {!ConnectivityCardElement}
   */
  function getConnectivityCard() {
    const connectivityCard =
        /** @type {!ConnectivityCardElement} */ (
            networkListElement.$$('connectivity-card'));
    assertTrue(!!connectivityCard);
    return connectivityCard;
  }

  test('ConnectivityCardInitialized', () => {
    return initializeNetworkList().then(
        () => assertTrue(!!getConnectivityCard()));
  });
}