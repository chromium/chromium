// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ethernet_info.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeEthernetNetwork} from 'chrome://diagnostics/fake_data.js';
import {AuthenticationType, EthernetStateProperties, Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDataPointHasExpectedHeaderAndValue, assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

suite('ethernetInfoTestSuite', function() {
  /** @type {?EthernetInfoElement} */
  let ethernetInfoElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    ethernetInfoElement.remove();
    ethernetInfoElement = null;
  });

  /**
   * @param {!Network} network
   */
  function initializeEthernetInfo(network) {
    assertFalse(!!ethernetInfoElement);

    // Add the ethernet info to the DOM.
    ethernetInfoElement =
        /** @type {!EthernetInfoElement} */ (
            document.createElement('ethernet-info'));
    assertTrue(!!ethernetInfoElement);
    ethernetInfoElement.network = network;
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
    const ethernetTypeProperies = Object.assign({}, baseProperties, typeProps);
    return Object.assign({}, fakeEthernetNetwork, {
      typeProperties: {
        ethernet: ethernetTypeProperies,
      },
    });
  }

  test('EthernetInfoIpAddressBasedOnNetwork', () => {
    return initializeEthernetInfo(fakeEthernetNetwork).then(() => {
      const expectedHeader = ethernetInfoElement.i18n('networkIpAddressLabel');
      assertDataPointHasExpectedHeaderAndValue(
          ethernetInfoElement, '#ipAddress', expectedHeader,
          fakeEthernetNetwork.ipConfig.ipAddress);
    });
  });

  test('EthernetInfoAuthenticationBasedOnNetwork', () => {
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

  test('EthernetInfoAuthenticationWithAuthentication', () => {
    return initializeEthernetInfo(
               getEthernetNetworkWithTypeProperties(
                   /** @type {EthernetStateProperties} */
                   ({authentication: AuthenticationType.kNone})))
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
});
