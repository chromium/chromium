// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/sim_lock_dialogs.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// clang-format on

suite('NetworkSimLockDialogsTest', function() {
  let simLockDialog;
  let unlockPinDialog;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let networkConfigRemote_ = null;

  setup(function() {
    networkConfigRemote_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ =
        networkConfigRemote_;

    simLockDialog = document.createElement('sim-lock-dialogs');
    simLockDialog.deviceState = {};
    document.body.appendChild(simLockDialog);
    Polymer.dom.flush();
  });

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Utility function used to check if a dialog with name |dialogName|
   * can be opened by setting the device state to |deviceState|.
   * @param {string} dialogName
   * @param {OncMojo.DeviceStateProperties} deviceState
   */
  async function verifyDialogShown(dialogName, deviceState) {
    const dialog = simLockDialog.$$(`#${dialogName}`);
    assertTrue(!!dialog);
    assertFalse(dialog.open);
    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertTrue(dialog.open);
  }

  test('Show Enter pin dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3}
    };
    verifyDialogShown('enterPinDialog', deviceState);
  });

  test('Show Change PIN dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 3}
    };
    simLockDialog.showChangePin = true;
    verifyDialogShown('changePinDialog', deviceState);
  });

  test('Show Unlock PIN dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3}
    };
    verifyDialogShown('unlockPinDialog', deviceState);
  });

  test('Show Unlock PUK dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3}
    };
    verifyDialogShown('unlockPukDialog', deviceState);
  });

  test('Show invalid unlock PIN error message properly', async function() {
    // Set sim to PIN locked state with multiple retries left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3}
    };
    await flushAsync();
    unlockPinDialog = simLockDialog.$$('#unlockPinDialog');
    assertTrue(!!unlockPinDialog);
    assertTrue(unlockPinDialog.open);

    // Invalid PIN should show error message with correct retries count.
    unlockPinDialog.querySelector('#unlockPin').value = 'invalid_pin';
    unlockPinDialog.querySelector('.action-button').click();
    await flushAsync();
    assertEquals(
        simLockDialog.i18n('networkSimErrorInvalidPinPlural', 3),
        unlockPinDialog.querySelector('.dialog-error').textContent.trim());

    // Set SIM to PIN locked state with single retry left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 1}
    };
    await flushAsync();
    unlockPinDialog.querySelector('#unlockPin').value = 'invalid_pin2';
    unlockPinDialog.querySelector('.action-button').click();
    await flushAsync();
    assertEquals(
        simLockDialog.i18n('networkSimErrorInvalidPin', 1),
        unlockPinDialog.querySelector('.dialog-error').textContent.trim());
  });

  test(
      'Show PUK dialog when lockType changes from PIN to PUK',
      async function() {
        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3}
        };
        await flushAsync();
        unlockPinDialog = simLockDialog.$$('#unlockPinDialog');
        assertTrue(!!unlockPinDialog);
        assertTrue(unlockPinDialog.open);

        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3}
        };
        await flushAsync();
        const unlockPukDialog = simLockDialog.$$('#unlockPukDialog');
        assertTrue(!!unlockPukDialog);

        assertFalse(unlockPinDialog.open);
        assertTrue(unlockPukDialog.open);
      });

  test(
      'Show unlock PUK dialog when lockEnabled changes to true from false',
      async function() {
        simLockDialog.deviceState = {
          simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3}
        };
        await flushAsync();
        const enterPinDialog = simLockDialog.$$('#enterPinDialog');
        assertTrue(!!enterPinDialog);
        assertTrue(enterPinDialog.open);

        // Set lockedEnabled to true, this means current network is locked and
        // enter pin dialog errored out.
        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 0}
        };

        const unlockPukDialog = simLockDialog.$$('#unlockPukDialog');

        assertTrue(!!unlockPukDialog);
        assertFalse(enterPinDialog.open);
        assertTrue(unlockPukDialog.open);
      });

  test('Show unlock PUK dialog when enter pin fails', async function() {
    const mojom = chromeos.networkConfig.mojom;
    let deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3}
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = simLockDialog.$$('#enterPinDialog');
    assertTrue(!!enterPinDialog);
    assertTrue(enterPinDialog.open);

    // Enter wrong pin 3 times.
    for (let i = 0; i < 3; i++) {
      // Change the pin for each entry, otherwise UI does not know its value
      // has changed.
      enterPinDialog.querySelector('#enterPin').value = '11111' + i;
      enterPinDialog.querySelector('.action-button').click();
      await flushAsync();
    }

    // Update device state with current device state.
    deviceState =
        networkConfigRemote_.getDeviceStateForTest(mojom.NetworkType.kCellular);
    // Trigger device state change.
    simLockDialog.deviceState = {...deviceState};
    await flushAsync();

    const unlockPukDialog = simLockDialog.$$('#unlockPukDialog');

    assertTrue(!!unlockPukDialog);
    assertFalse(enterPinDialog.open);
    assertTrue(unlockPukDialog.open);
  });

  test('Change pin', async function() {
    // Set sim to unlocked with multiple retries left
    const mojom = chromeos.networkConfig.mojom;
    let deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 2}
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.showChangePin = true;

    let changePinDialog = simLockDialog.$$('#changePinDialog');

    assertTrue(!!changePinDialog);
    assertFalse(changePinDialog.open);
    await flushAsync();

    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertTrue(changePinDialog.open);

    // Attempt to change pin.
    changePinDialog.querySelector('#changePinOld').value = '1111';
    changePinDialog.querySelector('#changePinNew1').value = '1234';
    changePinDialog.querySelector('#changePinNew2').value = '1234';
    changePinDialog.querySelector('.action-button').click();
    await flushAsync();

    assertEquals(networkConfigRemote_.testPin, '1234');
    assertFalse(changePinDialog.open);
  });

  test('Submit on enter pressed', async function() {
    const mojom = chromeos.networkConfig.mojom;
    let deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3}
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = simLockDialog.$$('#enterPinDialog');

    let pinInput = enterPinDialog.querySelector('#enterPin');
    pinInput.value = '1111111';
    pinInput.fire('enter', {path: [pinInput]});
    await flushAsync();

    deviceState =
        networkConfigRemote_.getDeviceStateForTest(mojom.NetworkType.kCellular);

    assertEquals(2, deviceState.simLockStatus.retriesLeft);
  });
});