// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cellular_info.js';

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
    document.body.appendChild(cellularInfoElement);

    return flushTasks();
  }

  test('CellularInfoPopulated', () => {
    return initializeCellularInfo().then(() => {
      dx_utils.assertElementContainsText(
          cellularInfoElement.$$('#cellularInfoContainer'), 'Cellular');
    });
  });
}