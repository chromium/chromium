// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/cellular_info.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {CellularInfoElement} from 'chrome://diagnostics/cellular_info.js';
import {getLockType, getSignalStrength} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeCellularNetwork} from 'chrome://diagnostics/fake_data.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDataPointHasExpectedHeaderAndValue} from './diagnostics_test_utils.js';

suite('cellularInfoTestSuite', function() {
  let cellularInfoElement: CellularInfoElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    cellularInfoElement?.remove();
    cellularInfoElement = null;
  });

  function initializeCellularInfo(network: Network): Promise<void> {
    assertFalse(!!cellularInfoElement);

    // Add the cellular info to the DOM.
    cellularInfoElement = document.createElement('cellular-info');
    assert(cellularInfoElement);
    cellularInfoElement.network = network;
    document.body.appendChild(cellularInfoElement);

    return flushTasks();
  }

  /**
   * Forces update to cellular network technology.
   */
  function setNetworkTechnology(networkTechnology: string): Promise<void> {
    assert(cellularInfoElement);

    const cellularTypeProps = Object.assign(
        {}, fakeCellularNetwork!.typeProperties!.cellular, {networkTechnology});
    cellularInfoElement.network = Object.assign({}, fakeCellularNetwork, {
      typeProperties:
          {cellular: cellularTypeProps, ethernet: undefined, wifi: undefined},
      ipConfig: {},
    });

    return flushTasks();
  }

  test('CellularInfoPopulated', () => {
    assert(fakeCellularNetwork);
    return initializeCellularInfo((fakeCellularNetwork as Network)).then(() => {
      assert(cellularInfoElement);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#ipAddress',
          cellularInfoElement!.i18n('networkIpAddressLabel'),
          `${fakeCellularNetwork!.ipConfig!.ipAddress}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#technology',
          cellularInfoElement!.i18n('networkTechnologyLabel'),
          `${
              fakeCellularNetwork!.typeProperties!.cellular!
                  .networkTechnology}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#roaming',
          cellularInfoElement!.i18n('networkRoamingStateLabel'),
          cellularInfoElement!.i18n('networkRoamingStateRoaming'));
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#signalStrength',
          cellularInfoElement!.i18n('networkSignalStrengthLabel'),
          getSignalStrength(
              fakeCellularNetwork!.typeProperties!.cellular!.signalStrength));
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#simLocked',
          cellularInfoElement!.i18n('networkSimLockStatusLabel'),
          cellularInfoElement!.i18n(
              'networkSimLockedText',
              getLockType(
                  fakeCellularNetwork!.typeProperties!.cellular!.lockType)));
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#iccid',
          cellularInfoElement!.i18n('networkIccidLabel'),
          `${fakeCellularNetwork!.typeProperties!.cellular!.iccid}`);
      assertDataPointHasExpectedHeaderAndValue(
          cellularInfoElement, '#eid',
          cellularInfoElement!.i18n('networkEidLabel'),
          `${fakeCellularNetwork!.typeProperties!.cellular!.eid}`);
    });
  });

  test('CellularNetworkTechnologyTranslated', () => {
    return initializeCellularInfo((fakeCellularNetwork as Network))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'CDMA1XRTT'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyCdma1xrttLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'EDGE'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyEdgeLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'EVDO'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyEvdoLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'GPRS'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyGprsLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'GSM'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyGsmLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'HSPA'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyHspaLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'HSPAPlus'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyHspaPlusLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'LTE'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyLteLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'LTEAdvanced'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyLteAdvancedLabel')))
        .then(() => setNetworkTechnology(/*networkTechnology=*/ 'UMTS'))
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'),
                cellularInfoElement!.i18n('networkTechnologyUmtsLabel')))
        // When typeProperties have not been set yet display empty string.
        .then(() => {
          assert(cellularInfoElement);
          cellularInfoElement.network =
              Object.assign({}, fakeCellularNetwork, {typeProperties: null});
          return flushTasks();
        })
        .then(
            () => assertDataPointHasExpectedHeaderAndValue(
                cellularInfoElement, '#technology',
                cellularInfoElement!.i18n('networkTechnologyLabel'), ''));
  });
});
