// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_siminfo.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import type {NetworkSiminfoElement} from 'chrome://resources/ash/common/network/network_siminfo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {SimLockDialogsElement} from 'chrome://resources/ash/common/network/sim_lock_dialogs.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import type {GlobalPolicy} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {InhibitReason, SuppressionType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('NetworkSiminfoTest', () => {
  let simInfo: NetworkSiminfoElement;

  const TEST_ICCID = '11111111111111111';

  setup(async () => {
    simInfo = document.createElement('network-siminfo');

    const cellularNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    cellularNetwork.typeState.cellular!.iccid = TEST_ICCID;

    simInfo.networkState = cellularNetwork;
    document.body.appendChild(simInfo);
    await flushTasks();
  });

  function getGlobalPolicy(allowCellularSimLock: boolean): GlobalPolicy {
    return {
      allowApnModification: false,
      allowOnlyPolicyWifiNetworksToConnect: false,
      allowCellularSimLock: allowCellularSimLock,
      allowCellularHotspot: false,
      allowOnlyPolicyCellularNetworks: false,
      allowOnlyPolicyNetworksToAutoconnect: false,
      allowOnlyPolicyWifiNetworksToConnectIfAvailable: false,
      dnsQueriesMonitored: false,
      reportXdrEventsEnabled: false,
      blockedHexSsids: [],
      recommendedValuesAreEphemeral: false,
      userCreatedNetworkConfigurationsAreEphemeral: false,
      allowTextMessages: SuppressionType.kUnset,
    };
  }

  async function updateDeviceState(
      isPrimary: boolean, lockEnabled: boolean,
      isLocked: boolean): Promise<void> {
    simInfo.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: '',
      macAddress: '',
      scanning: false,
      simLockStatus: {
        lockEnabled: lockEnabled,
        lockType: isLocked ? 'sim-pin' : '',
        retriesLeft: 3,
      },
      simInfos: [{
        iccid: TEST_ICCID,
        isPrimary: isPrimary,
        slotId: 0,
        eid: '',
      }],
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: '',
      isCarrierLocked: false,
      isFlashing: false,
    };
    await flushTasks();
  }

  /**
   * Verifies that the element with the provided ID exists and that clicking it
   * opens the SIM dialog. Also verifies that if the <network-siminfo> element
   * is disabled, this element is also disabled.
   */
  async function verifyExistsAndClickOpensDialog(elementId: string):
      Promise<void> {
    const getSimLockDialogElement = () =>
        simInfo.shadowRoot!.querySelector<SimLockDialogsElement>(
            'sim-lock-dialogs');

    // Element should exist.
    const element: CrButtonElement|CrToggleElement|null =
        simInfo.shadowRoot!.querySelector(`#${elementId}`);
    assertTrue(!!element);

    // If the <network-siminfo> element is disabled, this element should also be
    // disabled.
    simInfo.disabled = true;
    assertTrue(element.disabled);

    // Re-enable <network-siminfo>, and the element should become enabled again.
    simInfo.disabled = false;
    assertFalse(element.disabled);

    // SIM dialog is not shown.
    assertFalse(!!getSimLockDialogElement());

    // Click the element; this should cause the SIM dialog to be opened.
    element.click();
    await flushTasks();
    assertTrue(!!getSimLockDialogElement());
  }

  test('Set focus after dialog close', async () => {
    const getSimLockDialogs = () =>
        simInfo.shadowRoot!.querySelector<SimLockDialogsElement>(
            'sim-lock-dialogs');
    const getSimLockButton = () =>
        simInfo.shadowRoot!.querySelector<CrToggleElement>('#simLockButton');
    const getUnlockPinButton = () =>
        simInfo.shadowRoot!.querySelector<CrButtonElement>('#unlockPinButton');

    // SIM lock dialog toggle.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);
    assertFalse(!!getSimLockDialogs());
    const simLockButton = getSimLockButton();
    assertTrue(!!simLockButton);
    simLockButton.click();
    await flushTasks();

    assertTrue(!!getSimLockDialogs());
    // Simulate dialog close.
    let simLockDialogs = getSimLockDialogs();
    assertTrue(!!simLockDialogs);
    simLockDialogs.closeDialogsForTest();
    await flushTasks();

    assertFalse(!!getSimLockDialogs());
    assertEquals(getSimLockButton(), getDeepActiveElement());

    // SIM unlock pin button.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ true);
    await flushTasks();

    assertFalse(!!getSimLockDialogs());
    const unlockPinButton = getUnlockPinButton();
    assertTrue(!!unlockPinButton);
    unlockPinButton.click();
    await flushTasks();

    assertTrue(!!getSimLockDialogs());
    // Simulate dialog close.
    simLockDialogs = getSimLockDialogs();
    assertTrue(!!simLockDialogs);
    simLockDialogs.closeDialogsForTest();
    await flushTasks();

    assertFalse(!!getSimLockDialogs());
    assertEquals(getUnlockPinButton(), getDeepActiveElement());
  });

  test('Show sim lock dialog when unlock button is clicked', async () => {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ true);
    verifyExistsAndClickOpensDialog('unlockPinButton');
  });

  test('Show sim lock dialog when toggle is clicked', async () => {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);
    verifyExistsAndClickOpensDialog('simLockButton');
  });

  test('Show sim lock dialog when change button is clicked', () => {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);
    verifyExistsAndClickOpensDialog('changePinButton');
  });

  test('Policy controlled SIM lock setting', async () => {
    const getChangePinButton = () =>
        simInfo.shadowRoot!.querySelector<CrButtonElement>('#changePinButton');
    const getSimLockButton = () =>
        simInfo.shadowRoot!.querySelector<CrToggleElement>('#simLockButton');
    const getSimLockButtonTooltip = () =>
        simInfo.shadowRoot!.querySelector('#inActiveSimLockTooltip');
    const getSimLockPolicyIcon = () =>
        simInfo.shadowRoot!.querySelector('#simLockPolicyIcon');

    // No icon if policy does not disable SIM PIN locking.
    assertFalse(!!getSimLockPolicyIcon());

    simInfo.globalPolicy = getGlobalPolicy(false);
    await flushTasks();

    // Unlocked primary SIM with lock setting enabled. Change button should not
    // be visible, and toggle should be visible, on, and enabled to allow users
    // to turn off the SIM Lock setting.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);

    const changePinButton = getChangePinButton();
    assertTrue(!!changePinButton);
    assertTrue(changePinButton.hidden);

    const simLockButton = getSimLockButton();
    assertTrue(!!simLockButton);
    assertFalse(simLockButton.disabled);
    assertTrue(simLockButton.checked);

    assertFalse(!!getSimLockButtonTooltip());
    assertTrue(!!getSimLockPolicyIcon());

    // Policy controlled icon should not show if SIM PIN locking is not
    // restricted.
    simInfo.globalPolicy = getGlobalPolicy(true);
    await flushTasks();
    assertFalse(!!getSimLockPolicyIcon());

    simInfo.globalPolicy = getGlobalPolicy(false);
    await flushTasks();

    // Unlocked primary SIM with lock setting disabled. Change button should not
    // be visible, and toggle should be visible, off, and disabled to prevent
    // users to turn on the SIM Lock setting.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);

    assertTrue(changePinButton.hidden);
    assertTrue(simLockButton.disabled);
    assertFalse(simLockButton.checked);
    assertFalse(!!getSimLockButtonTooltip());
    assertTrue(!!getSimLockPolicyIcon());

    // Non-primary unlocked SIM with lock setting enabled. Change button should
    // be hidden, and toggle should be visible, off, and disabled.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(changePinButton.hidden);
    assertTrue(simLockButton.disabled);
    assertFalse(simLockButton.checked);
    assertTrue(!!getSimLockButtonTooltip());
    assertFalse(!!getSimLockPolicyIcon());

    // Non-primary unlocked SIM with lock setting disabled. Change button should
    // be hidden, and toggle should be visible, off, and disabled.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ false, /*isLocked=*/ false);
    assertTrue(changePinButton.hidden);
    assertTrue(simLockButton.disabled);
    assertFalse(simLockButton.checked);
    assertTrue(!!getSimLockButtonTooltip());
    assertFalse(!!getSimLockPolicyIcon());
  });

  test('Primary vs. non-primary SIM', () => {
    const getChangePinButton = () =>
        simInfo.shadowRoot!.querySelector<CrButtonElement>('#changePinButton');
    const getSimLockButton = () =>
        simInfo.shadowRoot!.querySelector<CrToggleElement>('#simLockButton');
    const getSimLockButtonTooltip = () =>
        simInfo.shadowRoot!.querySelector('#inActiveSimLockTooltip');

    // Lock enabled and primary slot; change button should be visible,
    // enabled, and checked.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(!!getChangePinButton());
    const simLockButton = getSimLockButton();
    assertTrue(!!simLockButton);
    assertFalse(simLockButton.disabled);
    assertTrue(simLockButton.checked);
    assertFalse(!!getSimLockButtonTooltip());

    // Lock enabled and non-primary slot; change button should be visible,
    // disabled, and unchecked.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(!!getChangePinButton());
    assertTrue(simLockButton.disabled);
    assertFalse(simLockButton.checked);
    assertTrue(!!getSimLockButtonTooltip());

    // SIM locked and non-primary slot; change button should be visible,
    // disabled, and unchecked.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ true);
    assertTrue(!!getChangePinButton());
    assertTrue(simLockButton.disabled);
    assertFalse(simLockButton.checked);
    assertTrue(!!getSimLockButtonTooltip());
  });
});
