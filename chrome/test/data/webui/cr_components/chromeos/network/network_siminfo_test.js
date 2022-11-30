// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_siminfo.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkSiminfoTest', function() {
  /** @type {!NetworkSiminfo|undefined} */
  let simInfo;

  const TEST_ICCID = '11111111111111111';

  setup(async function() {
    simInfo = document.createElement('network-simInfo');

    const cellularNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    cellularNetwork.typeState.cellular.iccid = TEST_ICCID;

    simInfo.networkState = cellularNetwork;
    document.body.appendChild(simInfo);
    await flushAsync();
  });

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   *
   * @param {boolean} isPrimary
   * @param {boolean} lockEnabled
   * @param {boolean} isLocked
   */
  async function updateDeviceState(isPrimary, lockEnabled, isLocked) {
    simInfo.deviceState = {
      simInfos: [{
        iccid: TEST_ICCID,
        isPrimary: isPrimary,
      }],
      simLockStatus: {
        lockEnabled: lockEnabled,
        lockType: isLocked ? 'sim-pin' : '',
        retriesLeft: 3,
      },
    };
    await flushAsync();
  }

  /**
   * Verifies that the element with the provided ID exists and that clicking it
   * opens the SIM dialog. Also verifies that if the <network-siminfo> element
   * is disabled, this element is also disabled.
   * @param {string} elementId
   */
  async function verifyExistsAndClickOpensDialog(elementId) {
    const getSimLockDialogElement = () => simInfo.$$('sim-lock-dialogs');

    // Element should exist.
    const element = simInfo.$$(`#${elementId}`);
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
    await flushAsync();
    assertTrue(!!getSimLockDialogElement());
  }

  test('Set focus after dialog close', async function() {
    const getSimLockDialogs = () => simInfo.$$('sim-lock-dialogs');
    const getSimLockButton = () => simInfo.$$('#simLockButton');
    const getUnlockPinButton = () => simInfo.$$('#unlockPinButton');

    // SIM lock dialog toggle.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);
    assertTrue(!!getSimLockButton());
    assertFalse(!!getSimLockDialogs());
    getSimLockButton().click();
    await flushAsync();

    assertTrue(!!getSimLockDialogs());

    // Simulate dialog close.
    getSimLockDialogs().closeDialogsForTest();
    await flushAsync();
    assertFalse(!!getSimLockDialogs());
    assertEquals(getSimLockButton(), getDeepActiveElement());

    // SIM unlock pin button.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ true);
    await flushAsync();
    assertTrue(!!getUnlockPinButton());
    assertFalse(!!getSimLockDialogs());
    getUnlockPinButton().click();
    await flushAsync();
    assertTrue(!!getSimLockDialogs());

    // Simulate dialog close.
    getSimLockDialogs().closeDialogsForTest();
    await flushAsync();
    assertFalse(!!getSimLockDialogs());
    assertEquals(getUnlockPinButton(), getDeepActiveElement());
  });

  test('Show sim lock dialog when unlock button is clicked', async function() {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ true);
    verifyExistsAndClickOpensDialog('unlockPinButton');
  });

  test('Show sim lock dialog when toggle is clicked', async function() {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);
    verifyExistsAndClickOpensDialog('simLockButton');
  });

  test('Show sim lock dialog when change button is clicked', function() {
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);
    verifyExistsAndClickOpensDialog('changePinButton');
  });

  test('Policy controlled SIM lock setting', async () => {
    const getChangePinButton = () => simInfo.$$('#changePinButton');
    const getSimLockButton = () => simInfo.$$('#simLockButton');
    const getSimLockButtonTooltip = () => simInfo.$$('#inActiveSimLockTooltip');
    const getSimLockPolicyIcon = () => simInfo.$$('#simLockPolicyIcon');

    // No icon if policy does not disable SIM PIN locking.
    assertFalse(!!getSimLockPolicyIcon());

    simInfo.globalPolicy = {
      allowCellularSimLock: false,
    };
    await flushAsync();

    // Unlocked primary SIM with lock setting enabled. Change button should not
    // be visible, and toggle should be visible, on, and enabled to allow users
    // to turn off the SIM Lock setting.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(getChangePinButton().hidden);
    assertFalse(getSimLockButton().disabled);
    assertTrue(getSimLockButton().checked);
    assertFalse(!!getSimLockButtonTooltip());
    assertTrue(!!getSimLockPolicyIcon());

    // Policy controlled icon should not show if SIM PIN locking is not
    // restricted.
    simInfo.globalPolicy = {
      allowCellularSimLock: true,
    };
    await flushAsync();
    assertFalse(!!getSimLockPolicyIcon());

    simInfo.globalPolicy = {
      allowCellularSimLock: false,
    };
    await flushAsync();

    // Unlocked primary SIM with lock setting disabled. Change button should not
    // be visible, and toggle should be visible, off, and disabled to prevent
    // users to turn on the SIM Lock setting.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ false, /*isLocked=*/ false);
    assertTrue(getChangePinButton().hidden);
    assertTrue(getSimLockButton().disabled);
    assertFalse(getSimLockButton().checked);
    assertFalse(!!getSimLockButtonTooltip());
    assertTrue(!!getSimLockPolicyIcon());

    // Non-primary unlocked SIM with lock setting enabled. Change button should
    // be hidden, and toggle should be visible, off, and disabled.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(getChangePinButton().hidden);
    assertTrue(getSimLockButton().disabled);
    assertFalse(getSimLockButton().checked);
    assertTrue(!!getSimLockButtonTooltip());
    assertFalse(!!getSimLockPolicyIcon());

    // Non-primary unlocked SIM with lock setting disabled. Change button should
    // be hidden, and toggle should be visible, off, and disabled.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ false, /*isLocked=*/ false);
    assertTrue(getChangePinButton().hidden);
    assertTrue(getSimLockButton().disabled);
    assertFalse(getSimLockButton().checked);
    assertTrue(!!getSimLockButtonTooltip());
    assertFalse(!!getSimLockPolicyIcon());
  });

  test('Primary vs. non-primary SIM', function() {
    const getChangePinButton = () => simInfo.$$('#changePinButton');
    const getSimLockButton = () => simInfo.$$('#simLockButton');
    const getSimLockButtonTooltip = () => simInfo.$$('#inActiveSimLockTooltip');

    // Lock enabled and primary slot; change button should be visible,
    // enabled, and checked.
    updateDeviceState(
        /*isPrimary=*/ true, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(!!getChangePinButton());
    assertFalse(getSimLockButton().disabled);
    assertTrue(getSimLockButton().checked);
    assertFalse(!!getSimLockButtonTooltip());

    // Lock enabled and non-primary slot; change button should be visible,
    // disabled, and unchecked.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ false);
    assertTrue(!!getChangePinButton());
    assertTrue(getSimLockButton().disabled);
    assertFalse(getSimLockButton().checked);
    assertTrue(!!getSimLockButtonTooltip());

    // SIM locked and non-primary slot; change button should be visible,
    // disabled, and unchecked.
    updateDeviceState(
        /*isPrimary=*/ false, /*lockEnabled=*/ true, /*isLocked=*/ true);
    assertTrue(!!getChangePinButton());
    assertTrue(getSimLockButton().disabled);
    assertFalse(getSimLockButton().checked);
    assertTrue(!!getSimLockButtonTooltip());
  });
});
