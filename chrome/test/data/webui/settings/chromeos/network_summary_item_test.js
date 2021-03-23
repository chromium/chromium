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

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: false,
      deviceState: {
        deviceState: mojom.DeviceStateType.kUninitialized,
        type: mojom.NetworkType.kEthernet,
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kEthernet,
      },
    });

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

  test('SIM info shown when locked but enabled, flag off', function() {
    const mojom = chromeos.networkConfig.mojom;

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: false,
      deviceState: {
        deviceState: mojom.DeviceStateType.kEnabled,
        type: mojom.NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: {lockType: 'sim-pin'},
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });

    Polymer.dom.flush();
    assertTrue(doesElementExist('network-siminfo'));
    assertFalse(doesElementExist('.subpage-arrow'));
  });

  test('Inhibited device on cellular network, flag on', function() {
    const mojom = chromeos.networkConfig.mojom;

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: true,
      deviceState: {
        inhibitReason: mojom.InhibitReason.kInstallingProfile,
        deviceState: mojom.DeviceStateType.kEnabled,
        type: mojom.NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });

    Polymer.dom.flush();
    assertTrue(netSummaryItem.$$('#deviceEnabledButton').checked);
    assertTrue(netSummaryItem.$$('#deviceEnabledButton').disabled);
  });

  test('Not inhibited device on cellular network, flag on', function() {
    const mojom = chromeos.networkConfig.mojom;

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: true,
      deviceState: {
        inhibitReason: mojom.InhibitReason.kNotInhibited,
        deviceState: mojom.DeviceStateType.kUnavailable,
        type: mojom.NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });

    Polymer.dom.flush();
    assertFalse(netSummaryItem.$$('#deviceEnabledButton').checked);
    assertFalse(netSummaryItem.$$('#deviceEnabledButton').disabled);
  });
});
