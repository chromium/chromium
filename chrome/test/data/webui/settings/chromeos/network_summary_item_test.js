// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
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

  function initWithPSimOnly(flagEnabled, isLocked) {
    const mojom = chromeos.networkConfig.mojom;
    const kTestIccid1 = '00000000000000000000';

    const simLockStatus = isLocked ? {lockType: 'sim-pin'} : {lockType: ''};

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: flagEnabled,
      deviceState: {
        deviceState: mojom.DeviceStateType.kEnabled,
        type: mojom.NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: simLockStatus,
        simInfos: [{slot_id: 1, eid: '', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });

    Polymer.dom.flush();
  }

  function initWithESimLocked(flagEnabled) {
    const mojom = chromeos.networkConfig.mojom;
    const kTestIccid1 = '00000000000000000000';

    netSummaryItem.setProperties({
      isUpdatedCellularUiEnabled_: flagEnabled,
      deviceState: {
        deviceState: mojom.DeviceStateType.kEnabled,
        type: mojom.NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: {lockType: 'sim-pin'},
        simInfos:
            [{slot_id: 1, eid: 'eid', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: mojom.ConnectionStateType.kNotConnected,
        guid: '',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });

    Polymer.dom.flush();
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

  test('Click event in SIMinfo should not trigger show details', function() {
    const mojom = chromeos.networkConfig.mojom;

    let showDetailFired = false;
    netSummaryItem.addEventListener(
        'show-detail', () => showDetailFired = true);

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
        guid: 'test_guid',
        type: mojom.NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}}
      },
    });
    Polymer.dom.flush();
    assertTrue(doesElementExist('network-siminfo'));

    const networkSimInfo = netSummaryItem.$$('network-siminfo');
    networkSimInfo.click();
    Polymer.dom.flush();
    assertFalse(showDetailFired);
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
    assertEquals(
        netSummaryItem.getNetworkStateText_(),
        netSummaryItem.i18n('internetDeviceBusy'));
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

  test('Mobile data toggle shown on locked device, flag on', function() {
    initWithESimLocked(/*flagEnabled = */ true);
    assertNotEquals(netSummaryItem.$$('#deviceEnabledButton'), null);
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('Mobile data toggle shown on locked device, flag off', function() {
    initWithESimLocked(/*flagEnabled = */ false);
    assertEquals(netSummaryItem.$$('#deviceEnabledButton'), null);
    assertFalse(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only locked device, show SIM locked UI, flag off', function() {
    initWithPSimOnly(/*flagEnabled = */ false, /*isLocked = */ true);
    assertTrue(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.$$('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(
        netSummaryItem.$$('#networkState').classList.contains('network-state'));
    assertFalse(doesElementExist('#deviceEnabledButton'));
  });

  test('eSIM enabled locked device, show SIM locked UI, flag off', function() {
    initWithESimLocked(/*flagEnabled = */ false);
    assertTrue(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.$$('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(
        netSummaryItem.$$('#networkState').classList.contains('network-state'));
    assertFalse(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only locked device, show SIM locked UI, flag on', function() {
    initWithPSimOnly(/*flagEnabled = */ true, /*isLocked = */ true);
    assertTrue(doesElementExist('network-siminfo'));
    assertTrue(netSummaryItem.$$('#networkState')
                   .classList.contains('locked-warning-message'));
    assertFalse(
        netSummaryItem.$$('#networkState').classList.contains('network-state'));
    assertFalse(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only locked device, no SIM locked UI, flag on', function() {
    initWithPSimOnly(/*flagEnabled = */ true, /*isLocked = */ false);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.$$('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(
        netSummaryItem.$$('#networkState').classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('eSIM enabled locked device, show SIM locked UI, flag on', function() {
    initWithESimLocked(/*flagEnabled = */ true);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.$$('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(
        netSummaryItem.$$('#networkState').classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test(
      'Show networks list when only 1 pSIM network is available',
      async function() {
        const mojom = chromeos.networkConfig.mojom;

        const showNetworksFiredPromise =
            test_util.eventToPromise('show-networks', netSummaryItem);

        // Simulate a device which has a single pSIM slot and no eSIM slots.
        const simInfos = [{slotId: 1, iccid: '000', isPrimary: true, eid: ''}];

        netSummaryItem.setProperties({
          isUpdatedCellularUiEnabled_: true,
          deviceState: {
            deviceState: mojom.DeviceStateType.kEnabled,
            type: mojom.NetworkType.kCellular,
            simAbsent: false,
            inhibitReason: mojom.InhibitReason.kNotInhibited,
            simLockStatus: {lockEnabled: false},
            simInfos: simInfos,
          },
          activeNetworkState: {
            connectionState: mojom.ConnectionStateType.kNotConnected,
            guid: '',
            type: mojom.NetworkType.kCellular,
            typeState: {cellular: {networkTechnology: ''}}
          },
        });
        Polymer.dom.flush();
        const networkState = netSummaryItem.$$('#networkState');
        assertTrue(!!networkState);
        networkState.click();
        Polymer.dom.flush();
        await showNetworksFiredPromise;
      });
});
