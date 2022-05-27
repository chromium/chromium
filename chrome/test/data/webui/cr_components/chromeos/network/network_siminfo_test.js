// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_siminfo.m.js';

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
        retriesLeft: 3
      }
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
