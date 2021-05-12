// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cellular_info.js';
import {fakeCellularNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function cellularInfoTestSuite() {
  /** @type {?CellularInfoElement} */
  let cellularInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    cellularInfoElement.remove();
    cellularInfoElement = null;
  });

  function initializeCellularInfo() {
    assertFalse(!!cellularInfoElement);

    // Add the cellular info to the DOM.
    cellularInfoElement =
        /** @type {!CellularInfoElement} */ (
            document.createElement('cellular-info'));
    assertTrue(!!cellularInfoElement);
    cellularInfoElement.network = fakeCellularNetwork;
    document.body.appendChild(cellularInfoElement);

    return flushTasks();
  }

  test('CellularInfoPopulated', () => {
    return initializeCellularInfo().then(() => {
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(cellularInfoElement, '#state'),
          `${fakeCellularNetwork.state}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(cellularInfoElement, '#name'),
          fakeCellularNetwork.name);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(cellularInfoElement, '#guid'),
          fakeCellularNetwork.guid);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(cellularInfoElement, '#macAddress'),
          fakeCellularNetwork.macAddress);
    });
  });
}