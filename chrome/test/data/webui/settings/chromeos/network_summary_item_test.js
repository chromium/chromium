// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkSummaryItem', function() {
  /** @type {!NetworkSummaryItemElement|undefined} */
  let netSummaryItem;

  // Returns true if the element exists and has not been 'removed' by the
  // Polymer template system.
  function doesElementExist(selector) {
    const el = netSummaryItem.$$(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  setup(function() {
    PolymerTest.clearBody();
    netSummaryItem = document.createElement('network-summary-item');
    document.body.appendChild(netSummaryItem);
    Polymer.dom.flush();
  });

  test('Device enabled button state', function() {
    const mojom = chromeos.networkConfig.mojom;

    netSummaryItem.deviceState = {
      deviceState: mojom.DeviceStateType.kUninitialized,
      type: mojom.NetworkType.kEthernet,
    };
    Polymer.dom.flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: mojom.DeviceStateType.kUninitialized,
      type: mojom.NetworkType.kVPN,
    };
    Polymer.dom.flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: mojom.DeviceStateType.kUninitialized,
      type: mojom.NetworkType.kTether,
    };
    Polymer.dom.flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: mojom.DeviceStateType.kUninitialized,
      type: mojom.NetworkType.kWiFi,
    };
    Polymer.dom.flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.setProperties({
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kConnected,
        guid: '',
        type: mojom.NetworkType.kWiFi,
        typeState: {
          wifi: {}
        }
      },
      deviceState: {
        deviceState: mojom.DeviceStateType.kEnabled,
        type: mojom.NetworkType.kWiFi,
      },
    });
    Polymer.dom.flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });
});
