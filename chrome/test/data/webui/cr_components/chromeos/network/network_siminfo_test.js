// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_siminfo.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// clang-format on

suite('NetworkSiminfoTest', function() {
  /** @type {!NetworkSiminfo|undefined} */
  let simInfo;

  const TEST_ICCID = '11111111111111111';
  const mojom = chromeos.networkConfig.mojom;

  setup(async function() {
    simInfo = document.createElement('network-simInfo');

    const cellularNetwork =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, 'cellular');
    cellularNetwork.typeState.cellular.iccid = TEST_ICCID;

    simInfo.networkState = cellularNetwork;
    simInfo.deviceState =
        createDeviceState(/*isPrimary=*/ true, /*lockEnabled=*/ true);

    document.body.appendChild(simInfo);
    await flushAsync();
  });

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   *
   * @param {boolean} isPrimary
   * @param {boolean} lockEnabled
   * @returns {OncMojo.DeviceStateProperties}
   */
  function createDeviceState(isPrimary, lockEnabled) {
    return {
      simInfos: [{
        iccid: TEST_ICCID,
        isPrimary: isPrimary,
      }],
      simLockStatus: {lockEnabled: lockEnabled, lockType: '', retriesLeft: 3}
    };
  }

  /**
   * Utility function used to check if a dialog with name |dialogName|
   * can be opened by setting the device state to |deviceState|
   * @param {string} dialogName
   * @param {OncMojo.DeviceStateProperties} deviceState
   */
  async function verifyDialogShown(buttonName) {
    let btn = simInfo.$$(`#${buttonName}`);
    assertTrue(!!btn);

    let simLockDialogs = simInfo.$$('sim-lock-dialogs');
    assertFalse(!!simLockDialogs);
    btn.click();
    await flushAsync();

    simLockDialogs = simInfo.$$('sim-lock-dialogs');
    assertTrue(!!simLockDialogs);
  }

  test('Show SIM missing', function() {
    // SIM missing UI is dependent on the device state being set.
    let simMissingGroup = simInfo.$$('#simMissing');
    assertTrue(simMissingGroup.hidden);

    // SIM lock status is not set on the device state, so the SIM is considered
    // missing.
    simInfo.deviceState = {};
    Polymer.dom.flush();
    assertFalse(simMissingGroup.hidden);

    // SIM lock status is set, so the SIM is not considered missing.
    simInfo.deviceState = {
      simLockStatus: {}
    };
    Polymer.dom.flush();
    assertTrue(simMissingGroup.hidden);
  });

  test('Show sim lock dialog when toggle is clicked', async function() {
    simInfo.deviceState =
        createDeviceState(/*isPrimary=*/ true, /*lockEnabled=*/ false);
    await flushAsync();
    verifyDialogShown('simLockButton');
  });

  test('Show sim lock dialog when change button is clicked', function() {
    verifyDialogShown('changePinButton');
  });

  test(
      'Hide change pin button and disable sim lock toggle if current slot is not primary',
      async function() {
        let changePinButton = simInfo.$$('#changePinButton');
        let simLockButton = simInfo.$$('#simLockButton');
        let simLockButtonTooltip = simInfo.$$('#inActiveSimLockTooltip');
        assertFalse(changePinButton.hidden);
        assertFalse(simLockButton.disabled);
        assertFalse(!!simLockButtonTooltip);

        // Trigger device state change
        simInfo.deviceState =
            createDeviceState(/*isPrimary=*/ false, /*lockEnabled=*/ true);
        await flushAsync();

        changePinButton = simInfo.$$('#changePinButton');
        simLockButton = simInfo.$$('#simLockButton');
        simLockButtonTooltip = simInfo.$$('#inActiveSimLockTooltip');

        assertTrue(!!simLockButtonTooltip);

        assertTrue(changePinButton.hidden);
        assertTrue(simLockButton.disabled);
        assertFalse(simLockButtonTooltip.hidden);
      });

  test('Disabled UI state', function() {
    const unlockPinButton = simInfo.$$('#unlockPinButton');
    const changePinButton = simInfo.$$('#changePinButton');
    const simLockButton = simInfo.$$('#simLockButton');

    assertFalse(unlockPinButton.disabled);
    assertFalse(changePinButton.disabled);
    assertFalse(simLockButton.disabled);

    simInfo.disabled = true;

    assertTrue(unlockPinButton.disabled);
    assertTrue(changePinButton.disabled);
    assertTrue(simLockButton.disabled);
  });
});