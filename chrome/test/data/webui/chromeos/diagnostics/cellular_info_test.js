// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cellular_info.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {getLockType, getSignalStrength} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeCellularNetwork} from 'chrome://diagnostics/fake_data.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertDataPointHasExpectedHeaderAndValue} from './diagnostics_test_utils.js';

suite('cellularInfoTestSuite', function() {
  /** @type {?CellularInfoElement} */
  let cellularInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    cellularInfoElement.remove();
    cellularInfoElement = null;
  });

  /**
   * @param {!Network} network
   */
  function initializeCellularInfo(network) {
    assertFalse(!!cellularInfoElement);

    // Add the cellular info to the DOM.
    cellularInfoElement =
        /** @type {!CellularInfoElement} */ (
            document.createElement('cellular-info'));
    assertTrue(!!cellularInfoElement);
    cellularInfoElement.network = network;
    document.body.appendChild(cellularInfoElement);

    return flushTasks();
  }

  /**
   * Forces update to cellular network technology.
   * @param {string} networkTechnology
   * @return {!Promise}
   */
  function setNetworkTechnology(networkTechnology) {
    assertTrue(!!cellularInfoElement);

    const cellularTypeProps = Object.assign(
        {}, fakeCellularNetwork.typeProperties.cellular, {networkTechnology});
    cellularInfoElement.network = Object.assign(
        {}, fakeCellularNetwork,
        {typeProperties: {cellular: cellularTypeProps}});

    return flushTasks();
  }

  test('CellularInfoPopulated', () => {
    return initializeCellularInfo(fakeCellularNetwork).then(() => {
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
          getSignalStrength(
              fakeCellularNetwork.typeProperties.cellular.signalStrength));
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

  test('CellularNetworkTechnologyTranslated', () => {
    return initializeCellularInfo()
        .then(() => setNetworkTechnology('CDMA1XRTT'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyCdma1xrttLabel')))
        .then(() => setNetworkTechnology('EDGE'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyEdgeLabel')))
        .then(() => setNetworkTechnology('EVDO'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyEvdoLabel')))
        .then(() => setNetworkTechnology('GPRS'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyGprsLabel')))
        .then(() => setNetworkTechnology('GSM'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyGsmLabel')))
        .then(() => setNetworkTechnology('HSPA'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyHspaLabel')))
        .then(() => setNetworkTechnology('HSPAPlus'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyHspaPlusLabel')))
        .then(() => setNetworkTechnology('LTE'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyLteLabel')))
        .then(() => setNetworkTechnology('LTEAdvanced'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyLteAdvancedLabel')))
        .then(() => setNetworkTechnology('UMTS'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'),
                cellularInfoElement.i18n('networkTechnologyUmtsLabel')))
        // When typeProperties have not been set yet display empty string.
        .then(() => {
          assertTrue(!!cellularInfoElement);
          cellularInfoElement.network =
              Object.assign({}, fakeCellularNetwork, {typeProperties: null});
          return flushTasks();
        })
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement.i18n('networkTechnologyLabel'), ''));
  });
});
