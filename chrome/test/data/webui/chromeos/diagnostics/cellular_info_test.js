// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cellular_info.js';

import {getLockType} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeCellularNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {assertDataPointHasExpectedHeaderAndValue} from './diagnostics_test_utils.js';

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
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#ipAddress',
          cellularInfoElement.i18n('networkIpAddressLabel'),
          `${fakeCellularNetwork.ipConfig.ipAddress}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#technology',
          cellularInfoElement.i18n('networkTechnologyLabel'),
          `${fakeCellularNetwork.typeProperties.cellular.networkTechnology}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#roaming',
          cellularInfoElement.i18n('networkRoamingStateLabel'),
          cellularInfoElement.i18n('networkRoamingStateRoaming'));
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#signalStrength',
          cellularInfoElement.i18n('networkSignalStrengthLabel'),
          fakeCellularNetwork.typeProperties.cellular.signalStrength);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#simLocked',
          cellularInfoElement.i18n('networkSimLockStatusLabel'),
          cellularInfoElement.i18n(
              'networkSimLockedText',
              getLockType(
                  fakeCellularNetwork.typeProperties.cellular.lockType)));
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#iccid',
          cellularInfoElement.i18n('networkIccidLabel'),
          `${fakeCellularNetwork.typeProperties.cellular.iccid}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#eid',
          cellularInfoElement.i18n('networkEidLabel'),
          `${fakeCellularNetwork.typeProperties.cellular.eid}`);
    });
  });
}
