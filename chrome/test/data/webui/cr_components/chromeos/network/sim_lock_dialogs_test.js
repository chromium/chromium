// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/sim_lock_dialogs.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('NetworkSimLockDialogsTest', function() {
  let simLockDialog;
  let unlockPinDialog;

  /** @type {?CrosNetworkConfigRemote} */
  let networkConfigRemote_ = null;

  setup(function() {
    networkConfigRemote_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigRemote_;

    simLockDialog = document.createElement('sim-lock-dialogs');
    simLockDialog.deviceState = {};
    document.body.appendChild(simLockDialog);
    flush();
  });

  async function flushAsync() {
    flush();
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
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    verifyDialogShown('enterPinDialog', deviceState);
    const enterPin = simLockDialog.$$(`#enterPin`);
    assertTrue(!!enterPin);
    assertEquals(
        enterPin.ariaLabel, simLockDialog.i18n('networkSimEnterPinTitle'));
  });

  test('Show Change PIN dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 3},
    };
    simLockDialog.showChangePin = true;
    verifyDialogShown('changePinDialog', deviceState);
  });

  test('Show Unlock PIN dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
    };
    verifyDialogShown('unlockPinDialog', deviceState);
    assertFalse(!!simLockDialog.$$(`#adminSubtitle`));
    simLockDialog.globalPolicy = {
      allowCellularSimLock: false,
    };
    await flushAsync();
    assertTrue(!!simLockDialog.$$(`#adminSubtitle`));
  });

  test('Unlock dialog not displayed when carrier locked', async function() {
    const deviceState = {
      simLockStatus:
          {lockEnabled: true, lockType: 'network-pin', retriesLeft: 3},
    };
    const dialog = simLockDialog.$$(`#unlockPinDialog`);
    assertTrue(!!dialog);
    assertFalse(dialog.open);
    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertFalse(dialog.open);
  });


  test('Show Unlock PUK dialog', async function() {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3},
    };
    verifyDialogShown('unlockPukDialog', deviceState);

    assertFalse(simLockDialog.$.unlockPin1.hidden);
    assertFalse(simLockDialog.$.unlockPin2.hidden);

    simLockDialog.globalPolicy = {
      allowCellularSimLock: false,
    };

    assertTrue(simLockDialog.$.unlockPin1.hidden);
    assertTrue(simLockDialog.$.unlockPin2.hidden);
  });

  test('Show invalid unlock PIN error message properly', async function() {
    // Set sim to PIN locked state with multiple retries left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
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
        unlockPinDialog.querySelector('.dialogSubtext').textContent.trim());

    // Set SIM to PIN locked state with single retry left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 1},
    };
    await flushAsync();
    unlockPinDialog.querySelector('#unlockPin').value = 'invalid_pin2';
    unlockPinDialog.querySelector('.action-button').click();
    await flushAsync();
    assertEquals(
        simLockDialog.i18n('networkSimErrorInvalidPin', 1),
        unlockPinDialog.querySelector('.dialogSubtext').textContent.trim());
  });

  test(
      'Show PUK dialog when lockType changes from PIN to PUK',
      async function() {
        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
        };
        await flushAsync();
        unlockPinDialog = simLockDialog.$$('#unlockPinDialog');
        assertTrue(!!unlockPinDialog);
        assertTrue(unlockPinDialog.open);

        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3},
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
          simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
        };
        await flushAsync();
        const enterPinDialog = simLockDialog.$$('#enterPinDialog');
        assertTrue(!!enterPinDialog);
        assertTrue(enterPinDialog.open);

        // Set lockedEnabled to true, this means current network is locked and
        // enter pin dialog errored out.
        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 0},
        };

        const unlockPukDialog = simLockDialog.$$('#unlockPukDialog');

        assertTrue(!!unlockPukDialog);
        assertFalse(enterPinDialog.open);
        assertTrue(unlockPukDialog.open);
      });

  test('Show unlock PUK dialog when enter pin fails', async function() {
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
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
        networkConfigRemote_.getDeviceStateForTest(NetworkType.kCellular);
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
    const deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 2},
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.showChangePin = true;

    const changePinDialog = simLockDialog.$$('#changePinDialog');

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
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = simLockDialog.$$('#enterPinDialog');

    const pinInput = enterPinDialog.querySelector('#enterPin');
    pinInput.value = '1111111';
    pinInput.dispatchEvent(new CustomEvent(
        'enter', {bubbles: true, composed: true, detail: {path: [pinInput]}}));
    await flushAsync();

    deviceState =
        networkConfigRemote_.getDeviceStateForTest(NetworkType.kCellular);

    assertEquals(2, deviceState.simLockStatus.retriesLeft);
  });

  test('Close dialog on cancel event pressed', async function() {
    // cancel event can be triggered by pressing the Escape key
    simLockDialog.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };

    await flushAsync();
    const enterPinDialog = simLockDialog.$$('#enterPinDialog');
    assertTrue(!!enterPinDialog);
    assertTrue(enterPinDialog.open);
    assertTrue(simLockDialog.isDialogOpen);
    enterPinDialog.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
    await flushAsync();
    assertFalse(enterPinDialog.open);
    assertFalse(simLockDialog.isDialogOpen);
  });

  test('Pending error is cleared', async function() {
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    networkConfigRemote_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = simLockDialog.$$('#enterPinDialog');
    const enterPin = async function(pin) {
      const pinInput = enterPinDialog.querySelector('#enterPin');
      pinInput.value = pin;
      pinInput.dispatchEvent(new CustomEvent(
          'enter',
          {bubbles: true, composed: true, detail: {path: [pinInput]}}));
      await flushAsync();
    };

    await enterPin('111111111');
    // Update device state.
    deviceState =
        networkConfigRemote_.getDeviceStateForTest(NetworkType.kCellular);
    simLockDialog.deviceState = {...deviceState};

    await flushAsync();
    let error =
        enterPinDialog.querySelector('.dialogSubtext').textContent.trim();
    assertEquals(
        error, simLockDialog.i18n('networkSimErrorIncorrectPinPlural', 2));

    await enterPin('1111');
    // Update device state.
    deviceState =
        networkConfigRemote_.getDeviceStateForTest(NetworkType.kCellular);
    simLockDialog.deviceState = {...deviceState};
    await flushAsync();

    error = enterPinDialog.querySelector('.dialogSubtext').textContent.trim();
    assertEquals(error, simLockDialog.i18n('networkSimEnterPinSubtext'));
  });
});
