// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_choose_mobile.js';

import {ConnectionStateType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkChooseMobileTest', function() {
  /** @type {!NetworkChooseMobile|undefined} */
  let chooseMobile;

  setup(function() {
    chooseMobile = document.createElement('network-choose-mobile');
    chooseMobile.managedProperties = {
      typeProperties: {
        cellular: {},
      },
    };
    document.body.appendChild(chooseMobile);
    flush();
  });

  test('Scan button enabled state', function() {
    const scanButton = chooseMobile.$$('cr-button');
    assertTrue(!!scanButton);
    assertTrue(scanButton.disabled);

    // A scan requires the connection state to be disconnected and the current
    // scan state to be 'not scanning'.
    chooseMobile.managedProperties = {
      connectionState: ConnectionStateType.kNotConnected,
      typeProperties: {
        cellular: {},
      },
    };
    chooseMobile.deviceState = {
      scanning: false,
    };
    flush();

    // Scan button is enabled.
    const isScanEnabled = !scanButton.disabled;
    assertTrue(isScanEnabled);

    // Set the device state to scanning.
    chooseMobile.deviceState = {
      scanning: true,
    };
    flush();

    // Scan button is disabled while the device is currently scanning.
    assertTrue(scanButton.disabled);

    // Reset scanning status.
    chooseMobile.deviceState = {
      scanning: false,
    };

    // Every connection state but kNotConnected prevents scanning.
    for (const state in ConnectionStateType) {
      if (state === ConnectionStateType.kNotConnected) {
        continue;
      }

      chooseMobile.managedProperties = {
        connectionState: state,
        typeProperties: {
          cellular: {},
        },
      };
      flush();

      assertTrue(scanButton.disabled);
    }
  });

  test('Disabled UI state', function() {
    chooseMobile.managedProperties = {
      connectionState: ConnectionStateType.kNotConnected,
      typeProperties: {
        cellular: {
          foundNetworks: [{
            networkId: '1',
            longName: 'network_name',
          }],
        },
      },
    };
    chooseMobile.deviceState = {scanning: false};
    flush();

    const scanButton = chooseMobile.$$('cr-button');
    const select = chooseMobile.$$('select');

    assertFalse(scanButton.disabled);
    assertFalse(select.disabled);

    chooseMobile.disabled = true;

    assertTrue(scanButton.disabled);
    assertTrue(select.disabled);
  });
});
