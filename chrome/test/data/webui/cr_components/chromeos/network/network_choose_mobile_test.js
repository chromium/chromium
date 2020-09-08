// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_choose_mobile.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkChooseMobileTest', function() {
  /** @type {!NetworkChooseMobile|undefined} */
  let chooseMobile;

  let mojom;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;

    chooseMobile = document.createElement('network-choose-mobile');
    chooseMobile.managedProperties = {
      typeProperties: {
        cellular: {}
      }
    };
    document.body.appendChild(chooseMobile);
    Polymer.dom.flush();
  });

  test('Scan button enabled state', function() {
    const scanButton = chooseMobile.$$('cr-button');
    assertTrue(!!scanButton);
    assertTrue(scanButton.disabled);

    // A scan requires the connection state to be disconnected and the current
    // scan state to be 'not scanning'.
    chooseMobile.managedProperties = {
      connectionState: mojom.ConnectionStateType.kNotConnected,
      typeProperties: {
        cellular: {}
      }
    };
    chooseMobile.deviceState = {
      scanning: false
    };
    Polymer.dom.flush();

    // Scan button is enabled.
    let isScanEnabled = !scanButton.disabled;
    assertTrue(isScanEnabled);

    // Set the device state to scanning.
    chooseMobile.deviceState = {
      scanning: true
    };
    Polymer.dom.flush();

    // Scan button is disabled while the device is currently scanning.
    assertTrue(scanButton.disabled);

    // Reset scanning status.
    chooseMobile.deviceState = {
      scanning: false
    };

    // Every connection state but kNotConnected prevents scanning.
    for (const state in mojom.ConnectionStateType) {
      if (state === mojom.ConnectionStateType.kNotConnected) continue;

      chooseMobile.managedProperties = {
        connectionState: state,
        typeProperties: {
          cellular: {}
        }
      };
      Polymer.dom.flush();

      assertTrue(scanButton.disabled);
    }
  });
});
