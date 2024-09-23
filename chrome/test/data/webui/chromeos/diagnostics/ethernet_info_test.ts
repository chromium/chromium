// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ethernet_info.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {EthernetNetwork} from 'chrome://diagnostics/diagnostics_types.js';
import {EthernetInfoElement} from 'chrome://diagnostics/ethernet_info.js';
import {fakeEthernetNetwork} from 'chrome://diagnostics/fake_data.js';
import {AuthenticationType, EthernetStateProperties, Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDataPointHasExpectedHeaderAndValue} from './diagnostics_test_utils.js';

suite('ethernetInfoTestSuite', function() {
  let ethernetInfoElement: EthernetInfoElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    ethernetInfoElement?.remove();
    ethernetInfoElement = null;
  });

  function initializeEthernetInfo(network: EthernetNetwork): Promise<void> {
    assertFalse(!!ethernetInfoElement);

    // Add the ethernet info to the DOM.
    ethernetInfoElement = document.createElement('ethernet-info');
    assert(ethernetInfoElement);
    ethernetInfoElement.network = network as Network;
    document.body.appendChild(ethernetInfoElement);

    return flushTasks();
  }

  function getEthernetNetworkWithTypeProperties(
      typeProps: EthernetStateProperties): Network {
    const baseProperties = {
      authentication: AuthenticationType.kNone,
    };
    const ethernetTypeProperies = Object.assign({}, baseProperties, typeProps);
    return Object.assign({}, fakeEthernetNetwork, {
      typeProperties: {
        ethernet: ethernetTypeProperies,
        cellular: undefined,
        wifi: undefined,
      },
      ipConfig: {},
    });
  }

  test('EthernetInfoIpAddressBasedOnNetwork', () => {
    return initializeEthernetInfo(fakeEthernetNetwork).then(() => {
      assert(ethernetInfoElement);
      const expectedHeader = ethernetInfoElement.i18n('networkIpAddressLabel');
      assert(fakeEthernetNetwork && fakeEthernetNetwork.ipConfig && fakeEthernetNetwork.ipConfig.ipAddress);
      const ipAddress = fakeEthernetNetwork.ipConfig.ipAddress;
      assertDataPointHasExpectedHeaderAndValue(
          ethernetInfoElement, '#ipAddress', expectedHeader, ipAddress);
    });
  });

  test('EthernetInfoAuthenticationBasedOnNetwork', () => {
    return initializeEthernetInfo(fakeEthernetNetwork).then(() => {
      assert(ethernetInfoElement);
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
                   {authentication: AuthenticationType.kNone}))
        .then(() => {
          assert(ethernetInfoElement);
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
