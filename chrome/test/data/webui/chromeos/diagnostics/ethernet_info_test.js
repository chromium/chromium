// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ethernet_info.js';

import {AuthenticationType, EthernetStateProperties, Network} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeEthernetNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {assertDataPointHasExpectedHeaderAndValue, assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

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

  /**
   * @param {!EthernetStateProperties} typeProps
   * @return {!Network}
   */
  function getEthernetNetworkWithTypeProperties(typeProps) {
    const baseProperties = {
      authentication: AuthenticationType.kNone,
    };
    const ethernetTypeProperties = Object.assign({}, baseProperties, typeProps);
    return Object.assign({}, fakeEthernetNetwork, {
      typeProperties: {
        ethernet: ethernetTypeProperties,
      }
    });
  }

  test('EthernetInfoPopulated', () => {
    return initializeEthernetInfo().then(() => {
      // Element expected on screen but data currently missing in api.
      // TODO(ashleydp): Update test when link speed data-point value provided.
      assertTextContains(
          getDataPointValue(ethernetInfoElement, '#linkSpeed'), '');
    });
  });

  test('EthernetInfoIpAddressBasedOnNetwork', () => {
    return initializeEthernetInfo().then(() => {
      const expectedHeader = ethernetInfoElement.i18n('networkIpAddressLabel');
      assertDataPointHasExpectedHeaderAndValue(
          ethernetInfoElement, '#ipAddress', expectedHeader,
          fakeEthernetNetwork.ipConfig.ipAddress);
    });
  });

  test('EthernetInfoAuthenticationWithAuthentication8021x', () => {
    return initializeEthernetInfo(fakeEthernetNetwork).then(() => {
      const expectedHeader =
          ethernetInfoElement.i18n('networkAuthenticationLabel');
      const expectedValue =
          ethernetInfoElement.i18n('networkEthernetAuthentication8021xLabel');
      assertDataPointHasExpectedHeaderAndValue(
          ethernetInfoElement, '#authentication', expectedHeader,
          expectedValue);
    });
  });

  test('EthernetInfoAuthenticationWithAuthenticationNone', () => {
    return initializeEthernetInfo()
        .then(() => {
          ethernetInfoElement.network = getEthernetNetworkWithTypeProperties(
              /** @type {EthernetStateProperties} */
              ({authentication: AuthenticationType.kNone}));
          return flushTasks();
        })
        .then(() => {
          const expectedHeader =
              ethernetInfoElement.i18n('networkAuthenticationLabel');
          const expectedValue = ethernetInfoElement.i18n(
              'networkEthernetAuthenticationNoneLabel');
          assertDataPointHasExpectedHeaderAndValue(
              ethernetInfoElement, '#authentication', expectedHeader,
              expectedValue);
        });
  });

  test('EthernetInfoLinkSpeedBasedOnNetwork', () => {
    return initializeEthernetInfo(fakeEthernetNetwork).then(() => {
      const expectedHeader = ethernetInfoElement.i18n('networkLinkSpeedLabel');
      // TODO(ashleydp): Update test when Ethernet link speed data provided.
      assertDataPointHasExpectedHeaderAndValue(
          ethernetInfoElement, '#linkSpeed', expectedHeader, '');
    });
  });
}
