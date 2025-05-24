// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/sim_lock_dialogs.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkPasswordInputElement} from 'chrome://resources/ash/common/network/network_password_input.js';
import type {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {SimLockDialogsElement} from 'chrome://resources/ash/common/network/sim_lock_dialogs.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import type {GlobalPolicy} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

suite('NetworkSimLockDialogsTest', () => {
  let simLockDialog: SimLockDialogsElement;
  let unlockPinDialog: CrDialogElement;

  let mojoApi_: FakeNetworkConfig;

  setup(() => {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);

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
   */
  async function verifyDialogShown(
      dialogName: string,
      deviceState: OncMojo.DeviceStateProperties): Promise<void> {
    const dialog = strictQuery(
        `#${dialogName}`, simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!dialog);
    assertFalse(dialog.open);
    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertTrue(dialog.open);
  }

  test('Show Enter pin dialog', () => {
    const deviceState = {
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    verifyDialogShown('enterPinDialog', deviceState);
    const enterPin = strictQuery(
        '#enterPin', simLockDialog.shadowRoot, NetworkPasswordInputElement);
    assertTrue(!!enterPin);
    assertEquals(
        enterPin.ariaLabel, simLockDialog.i18n('networkSimEnterPinTitle'));
  });

  test('Show Change PIN dialog', () => {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 3},
    };
    simLockDialog.showChangePin = true;
    verifyDialogShown('changePinDialog', deviceState);
  });

  test('Show Unlock PIN dialog', async () => {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
    };
    verifyDialogShown('unlockPinDialog', deviceState);
    assertFalse(!!simLockDialog.shadowRoot!.querySelector<HTMLElement>(
        '#adminSubtitle'));
    const globalPolicy = {
      allowCellularSimLock: false,
    } as GlobalPolicy;
    simLockDialog.globalPolicy = globalPolicy;
    await flushAsync();
    assertTrue(
        !!strictQuery('#adminSubtitle', simLockDialog.shadowRoot, HTMLElement));
  });

  test('Unlock dialog not displayed when carrier locked', async () => {
    const deviceState = {
      simLockStatus:
          {lockEnabled: true, lockType: 'network-pin', retriesLeft: 3},
    };
    const dialog = strictQuery(
        '#unlockPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!dialog);
    assertFalse(dialog.open);
    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertFalse(dialog.open);
  });


  test('Show Unlock PUK dialog', () => {
    const deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3},
    };
    verifyDialogShown('unlockPukDialog', deviceState);

    assertFalse(strictQuery(
                    '#unlockPin1', simLockDialog.shadowRoot,
                    NetworkPasswordInputElement)
                    .hidden);
    assertFalse(strictQuery(
                    '#unlockPin2', simLockDialog.shadowRoot,
                    NetworkPasswordInputElement)
                    .hidden);

    const globalPolicy = {
      allowCellularSimLock: false,
    } as GlobalPolicy;
    simLockDialog.globalPolicy = globalPolicy;

    assertTrue(strictQuery(
                   '#unlockPin1', simLockDialog.shadowRoot,
                   NetworkPasswordInputElement)
                   .hidden);
    assertTrue(strictQuery(
                   '#unlockPin2', simLockDialog.shadowRoot,
                   NetworkPasswordInputElement)
                   .hidden);
  });

  test('Show invalid unlock PIN error message properly', async () => {
    // Set sim to PIN locked state with multiple retries left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
    };
    await flushAsync();
    unlockPinDialog = strictQuery(
        '#unlockPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!unlockPinDialog);
    assertTrue(unlockPinDialog.open);

    // Invalid PIN should show error message with correct retries count.
    strictQuery('#unlockPin', unlockPinDialog, NetworkPasswordInputElement)
        .value = 'invalid_pin';
    strictQuery('.action-button', unlockPinDialog, CrButtonElement).click();
    await flushAsync();
    assertEquals(
        simLockDialog.i18n('networkSimErrorInvalidPinPlural', 3),
        strictQuery('.dialogSubtext', unlockPinDialog, HTMLElement)
            .textContent!.trim());

    // Set SIM to PIN locked state with single retry left.
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 1},
    };
    await flushAsync();
    strictQuery('#unlockPin', unlockPinDialog, NetworkPasswordInputElement)
        .value = 'invalid_pin2';
    strictQuery('.action-button', unlockPinDialog, CrButtonElement).click();
    await flushAsync();
    assertEquals(
        simLockDialog.i18n('networkSimErrorInvalidPin', 1),
        strictQuery('.dialogSubtext', unlockPinDialog, HTMLElement)
            .textContent!.trim());
  });

  test('Show PUK dialog when lockType changes from PIN to PUK', async () => {
    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-pin', retriesLeft: 3},
    };
    await flushAsync();
    unlockPinDialog = strictQuery(
        '#unlockPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!unlockPinDialog);
    assertTrue(unlockPinDialog.open);

    simLockDialog.deviceState = {
      simLockStatus: {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 3},
    };
    await flushAsync();
    const unlockPukDialog = strictQuery(
        '#unlockPukDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!unlockPukDialog);

    assertFalse(unlockPinDialog.open);
    assertTrue(unlockPukDialog.open);
  });

  test(
      'Show unlock PUK dialog when lockEnabled changes to true from false',
      async () => {
        simLockDialog.deviceState = {
          simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
        };
        await flushAsync();
        const enterPinDialog = strictQuery(
            '#enterPinDialog', simLockDialog.shadowRoot, CrDialogElement);
        assertTrue(!!enterPinDialog);
        assertTrue(enterPinDialog.open);

        // Set lockedEnabled to true, this means current network is locked and
        // enter pin dialog errored out.
        simLockDialog.deviceState = {
          simLockStatus:
              {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 0},
        };

        const unlockPukDialog = strictQuery(
            '#unlockPukDialog', simLockDialog.shadowRoot, CrDialogElement);

        assertTrue(!!unlockPukDialog);
        assertFalse(enterPinDialog.open);
        assertTrue(unlockPukDialog.open);
      });

  test('Show unlock PUK dialog when enter pin fails', async () => {
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    mojoApi_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = strictQuery(
        '#enterPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!enterPinDialog);
    assertTrue(enterPinDialog.open);

    // Enter wrong pin 3 times.
    for (let i = 0; i < 3; i++) {
      // Change the pin for each entry, otherwise UI does not know its value
      // has changed.
      strictQuery('#enterPin', enterPinDialog, NetworkPasswordInputElement)
          .value = '11111' + i;
      strictQuery('.action-button', enterPinDialog, CrButtonElement).click();
      await flushAsync();
    }

    // Update device state with current device state.
    deviceState = mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    // Trigger device state change.
    simLockDialog.deviceState = {...deviceState};
    await flushAsync();

    const unlockPukDialog = strictQuery(
        '#unlockPukDialog', simLockDialog.shadowRoot, CrDialogElement);

    assertTrue(!!unlockPukDialog);
    assertFalse(enterPinDialog.open);
    assertTrue(unlockPukDialog.open);
  });

  test('Change pin', async () => {
    // Set sim to unlocked with multiple retries left
    const deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: true, lockType: '', retriesLeft: 2},
    };
    mojoApi_.setDeviceStateForTest(deviceState);
    simLockDialog.showChangePin = true;

    const changePinDialog = strictQuery(
        '#changePinDialog', simLockDialog.shadowRoot, CrDialogElement);

    assertTrue(!!changePinDialog);
    assertFalse(changePinDialog.open);
    await flushAsync();

    simLockDialog.deviceState = deviceState;
    await flushAsync();
    assertTrue(changePinDialog.open);

    // Attempt to change pin.
    strictQuery('#changePinOld', changePinDialog, NetworkPasswordInputElement)
        .value = '1111';
    strictQuery('#changePinNew1', changePinDialog, NetworkPasswordInputElement)
        .value = '1234';
    strictQuery('#changePinNew2', changePinDialog, NetworkPasswordInputElement)
        .value = '1234';
    strictQuery('.action-button', changePinDialog, CrButtonElement).click();
    await flushAsync();

    assertEquals(mojoApi_.testPin, '1234');
    assertFalse(changePinDialog.open);
  });

  test('Submit on enter pressed', async () => {
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    mojoApi_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = strictQuery(
        '#enterPinDialog', simLockDialog.shadowRoot, CrDialogElement);

    const pinInput =
        strictQuery('#enterPin', enterPinDialog, NetworkPasswordInputElement);
    pinInput.value = '1111111';
    pinInput.dispatchEvent(new CustomEvent(
        'enter', {bubbles: true, composed: true, detail: {path: [pinInput]}}));
    await flushAsync();

    deviceState = mojoApi_.getDeviceStateForTest(NetworkType.kCellular);

    assertEquals(2, deviceState.simLockStatus.retriesLeft);
  });

  test('Close dialog on cancel event pressed', async () => {
    // cancel event can be triggered by pressing the Escape key
    simLockDialog.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };

    await flushAsync();
    const enterPinDialog = strictQuery(
        '#enterPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    assertTrue(!!enterPinDialog);
    assertTrue(enterPinDialog.open);
    assertTrue(simLockDialog.isDialogOpen);
    enterPinDialog.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
    await flushAsync();
    assertFalse(enterPinDialog.open);
    assertFalse(simLockDialog.isDialogOpen);
  });

  test('Pending error is cleared', async () => {
    let deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
      simLockStatus: {lockEnabled: false, lockType: '', retriesLeft: 3},
    };
    mojoApi_.setDeviceStateForTest(deviceState);
    simLockDialog.deviceState = deviceState;
    await flushAsync();

    const enterPinDialog = strictQuery(
        '#enterPinDialog', simLockDialog.shadowRoot, CrDialogElement);
    const enterPin = async function(pin: string): Promise<void> {
      const pinInput =
          strictQuery('#enterPin', enterPinDialog, NetworkPasswordInputElement);
      assertTrue(!!pinInput);
      pinInput.value = pin;
      pinInput.dispatchEvent(new CustomEvent(
          'enter',
          {bubbles: true, composed: true, detail: {path: [pinInput]}}));
      await flushAsync();
    };

    await enterPin('111111111');
    // Update device state.
    deviceState = mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    simLockDialog.deviceState = {...deviceState};

    await flushAsync();
    const error = strictQuery('.dialogSubtext', enterPinDialog, HTMLElement);
    assertEquals(
        error.textContent?.trim(),
        simLockDialog.i18n('networkSimErrorIncorrectPinPlural', 2));

    await enterPin('1111');
    // Update device state.
    deviceState = mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    simLockDialog.deviceState = {...deviceState};
    await flushAsync();

    assertEquals(
        error.textContent?.trim(),
        simLockDialog.i18n('networkSimEnterPinSubtext'));
  });
});
