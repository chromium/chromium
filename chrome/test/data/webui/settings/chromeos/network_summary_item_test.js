// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://test/test_util.js';

suite('NetworkSummaryItem', function() {
  /** @type {!NetworkSummaryItemElement|undefined} */
  let netSummaryItem;

  // Returns true if the element exists and has not been 'removed' by the
  // Polymer template system.
  function doesElementExist(selector) {
    const el = netSummaryItem.shadowRoot.querySelector(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  function initWithPSimOnly(isLocked) {
    const kTestIccid1 = '00000000000000000000';

    const simLockStatus = isLocked ? {lockType: 'sim-pin'} : {lockType: ''};

    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: simLockStatus,
        simInfos: [{slot_id: 1, eid: '', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
  }

  function initWithESimLocked() {
    const kTestIccid1 = '00000000000000000000';

    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: {lockType: 'sim-pin'},
        simInfos:
            [{slot_id: 1, eid: 'eid', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
  }

  setup(function() {
    PolymerTest.clearBody();
    netSummaryItem = document.createElement('network-summary-item');
    document.body.appendChild(netSummaryItem);
    flush();
  });

  test('Device enabled button state', function() {
    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kUninitialized,
        type: NetworkType.kEthernet,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kEthernet,
      },
    });

    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kVPN,
    };
    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kTether,
    };
    flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kWiFi,
    };
    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.setProperties({
      activeNetworkState: {
        connectionState: ConnectionStateType.kConnected,
        guid: '',
        type: NetworkType.kWiFi,
        typeState: {
          wifi: {},
        },
      },
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kWiFi,
      },
    });
    flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('Inhibited device on cellular network', function() {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kInstallingProfile,
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertTrue(netSummaryItem.shadowRoot.querySelector('#deviceEnabledButton')
                   .checked);
    assertTrue(netSummaryItem.shadowRoot.querySelector('#deviceEnabledButton')
                   .disabled);
    assertEquals(
        netSummaryItem.getNetworkStateText_(),
        netSummaryItem.i18n('internetDeviceBusy'));
  });

  test('Not inhibited device on cellular network', function() {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kNotInhibited,
        deviceState: DeviceStateType.kUnavailable,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertFalse(netSummaryItem.shadowRoot.querySelector('#deviceEnabledButton')
                    .checked);
    assertFalse(netSummaryItem.shadowRoot.querySelector('#deviceEnabledButton')
                    .disabled);
  });

  test('Mobile data toggle shown on locked device', function() {
    initWithESimLocked();
    assertNotEquals(
        netSummaryItem.shadowRoot.querySelector('#deviceEnabledButton'), null);
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only locked device, show SIM locked UI', function() {
    initWithPSimOnly(/*isLocked=*/ true);
    assertTrue(doesElementExist('network-siminfo'));
    assertTrue(netSummaryItem.shadowRoot.querySelector('#networkState')
                   .classList.contains('locked-warning-message'));
    assertFalse(netSummaryItem.shadowRoot.querySelector('#networkState')
                    .classList.contains('network-state'));
    assertFalse(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only locked device, no SIM locked UI', function() {
    initWithPSimOnly(/*isLocked=*/ false);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot.querySelector('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(netSummaryItem.shadowRoot.querySelector('#networkState')
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('eSIM enabled locked device, show SIM locked UI', function() {
    initWithESimLocked();
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot.querySelector('#networkState')
                    .classList.contains('locked-warning-message'));
    assertTrue(netSummaryItem.shadowRoot.querySelector('#networkState')
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test(
      'Show networks list when only 1 pSIM network is available',
      async function() {
        const showNetworksFiredPromise =
            eventToPromise('show-networks', netSummaryItem);

        // Simulate a device which has a single pSIM slot and no eSIM slots.
        const simInfos = [{slotId: 1, iccid: '000', isPrimary: true, eid: ''}];

        netSummaryItem.setProperties({
          deviceState: {
            deviceState: DeviceStateType.kEnabled,
            type: NetworkType.kCellular,
            simAbsent: false,
            inhibitReason: InhibitReason.kNotInhibited,
            simLockStatus: {lockEnabled: false},
            simInfos: simInfos,
          },
          activeNetworkState: {
            connectionState: ConnectionStateType.kNotConnected,
            guid: '',
            type: NetworkType.kCellular,
            typeState: {cellular: {networkTechnology: ''}},
          },
        });
        flush();
        const networkState =
            netSummaryItem.shadowRoot.querySelector('#networkState');
        assertTrue(!!networkState);
        networkState.click();
        flush();
        await showNetworksFiredPromise;
      });
});
