// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function wifiInfoTestSuite() {
  /** @type {?WifiInfoElement} */
  let wifiInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    wifiInfoElement.remove();
    wifiInfoElement = null;
  });

  function initializeWifiInfo() {
    assertFalse(!!wifiInfoElement);

    // Add the wifi info to the DOM.
    wifiInfoElement =
        /** @type {!WifiInfoElement} */ (document.createElement('wifi-info'));
    assertTrue(!!wifiInfoElement);
    document.body.appendChild(wifiInfoElement);

    return flushTasks();
  }

  test('WifiInfoPopulated', () => {
    return initializeWifiInfo().then(() => {
      dx_utils.assertElementContainsText(
          wifiInfoElement.$$('#wifiInfoContainer'), 'WiFi');
    });
  });
}