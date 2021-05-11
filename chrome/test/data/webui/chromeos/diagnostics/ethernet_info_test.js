// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ethernet_info.js';
import {fakeEthernetNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function ethernetInfoTestSuite() {
  /** @type {?EthernetInfoElement} */
  let ethernetInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    ethernetInfoElement.remove();
    ethernetInfoElement = null;
  });

  function initializeEthernetInfo() {
    assertFalse(!!ethernetInfoElement);

    // Add the ethernet info to the DOM.
    ethernetInfoElement =
        /** @type {!EthernetInfoElement} */ (
            document.createElement('ethernet-info'));
    assertTrue(!!ethernetInfoElement);
    ethernetInfoElement.network = fakeEthernetNetwork;
    document.body.appendChild(ethernetInfoElement);

    return flushTasks();
  }

  test('EthernetInfoPopulated', () => {
    return initializeEthernetInfo().then(() => {
      const dataPoints = dx_utils.getDataPointElements(
          ethernetInfoElement, '#ethernetInfoContainer');
      dx_utils.assertTextContains(
          `${dataPoints[0].value}`, `${fakeEthernetNetwork.state}`);
      dx_utils.assertTextContains(
          dataPoints[1].value, fakeEthernetNetwork.name);
      dx_utils.assertTextContains(
          dataPoints[2].value, fakeEthernetNetwork.guid);
      dx_utils.assertTextContains(
          dataPoints[3].value, fakeEthernetNetwork.macAddress);
    });
  });
}